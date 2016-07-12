/* gstreamermm - a C++ wrapper for gstreamer
 *
 * Copyright 2009 The gstreamermm Development Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <gstreamermm.h>
#include <glibmm.h>
#include <iostream>
#include <type_traits>

typedef struct _GstTypeFindElement GstTypeFindElement;

Glib::RefPtr<Glib::MainLoop> mainloop;
bool have_type = false;

static bool on_structure_foreach(const Glib::QueryQuark& id, const Glib::ValueBase& value)
{
  const Glib::ustring str_id = id;
  gchar* str_value = g_strdup_value_contents(value.gobj());
  std::cout << "Structure field: id=" << str_id << ", value=" << str_value << std::endl;
  g_free(str_value);

  return true;
}

namespace Glib{
template <typename T>
constexpr T wrap (const T& v, bool=true)
{
  return v;
}

template <typename T>
constexpr T unwrap (const T& v, bool=true)
{
  return v;
}
}

template<typename ...T>
static void signal_callback(void* self, T ...v)
{
  using namespace Gst;
  typedef sigc::slot< void, decltype(Glib::wrap(v))... > SlotType;

  void* data = std::get<sizeof...(T)-1>(std::tuple<T...>(v...));

  try
  {
    if(sigc::slot_base *const slot = Glib::SignalProxyNormal::data_to_slot(data))
    {
      (*static_cast<SlotType*>(slot))(Glib::wrap(std::forward<T>(v), true)...);
    }
  }
  catch(...)
  {
     Glib::exception_handlers_invoke();
  }
}


template<typename ...T>
Glib::SignalProxy< void, T... > signal_have_type(const Glib::RefPtr<Glib::Object>& obj)
{

  static const Glib::SignalProxyInfo signal_info =
  {
    "have-type",
    (GCallback) &signal_callback<decltype(Glib::unwrap(*((typename std::remove_reference<T>::type*)nullptr)))..., void*>,
    (GCallback) &signal_callback<decltype(Glib::unwrap(*((typename std::remove_reference<T>::type*)nullptr)))..., void*>
  };

  return Glib::SignalProxy< void, T... >(obj.operator->(), &signal_info);
}

static void on_typefind_have_type(guint probability, const Glib::RefPtr<Gst::Caps>& caps)
{
  have_type = true;

  std::cout << "have-type: probability=" << probability << std::endl;

  if(!caps)
  {
    std::cerr << "on_typefind_have_type(): caps is null" << std::endl;
    return;
  }

  if(!caps->size())
  {
    std::cerr << "on_typefind_have_type(): caps is empty" << std::endl;
    return;
  }

  Gst::Structure structure = caps->get_structure(0);
  const Glib::ustring mime_type = structure.get_name();
  std::cout << "have-type: mime_type=" << mime_type << std::endl;

  structure.foreach( sigc::ptr_fun(&on_structure_foreach) );

  if(mainloop)
    mainloop->quit();
  else
    std::cerr << "on_typefind_have_type(): mainloop is null" << std::endl;
}

int main(int argc, char** argv)
{
  // Initialize Gstreamermm:
  Gst::init(argc, argv);

  //Check input arguments:
  if(argc < 2)
  {
    std::cout << "Usage: " << argv[0] << " <filename>" << std::endl;
    return 1;
  }

  const std::string filename = argv[1];

  // Create pipeline:
  Glib::RefPtr<Gst::Pipeline> pipeline = Gst::Pipeline::create("my-pipeline");

  // Create elements:
  Glib::RefPtr<Gst::Element> element_source = Gst::ElementFactory::create_element("filesrc");
  element_source->set_property("location", filename);

  //If using an MP3 file, this should provide more Caps information from typefind.
  //Glib::RefPtr<Gst::Element> element_id3demux = Gst::ElementFactory::create_element("id3demux");
  //g_assert(element_id3demux);

  Glib::RefPtr<Gst::Element> element_typefind = Gst::ElementFactory::create_element("typefind");
  signal_have_type<guint, const Glib::RefPtr<Gst::Caps>&>(element_typefind).connect(
    sigc::ptr_fun(&on_typefind_have_type) );
  Glib::RefPtr<Gst::Element> element_sink = Gst::ElementFactory::create_element("fakesink");

  // We must add the elements to the pipeline before linking them:
  try
  {
    pipeline->add(element_source)->add(element_typefind)->add(element_sink);
    //pipeline->add(element_source)->add(element_id3demux)->add(element_typefind)->add(element_sink);
  }
  catch (std::runtime_error& ex)
  {
    std::cerr << "Exception while adding: " << ex.what() << std::endl;
    return 1;
  }

  // Link the elements together:
  try
  {
    //element_source->link(element_id3demux)->link(element_typefind)->link(element_sink);
    element_source->link(element_typefind)->link(element_sink);
  }
  catch(const std::runtime_error& error)
  {
    std::cerr << "Exception while linking: " << error.what() << std::endl;
    return 1;
  }

  // Now set the whole pipeline to playing and start the main loop:
  std::cout << "Setting pipeline to PLAYING." << std::endl;
  mainloop = Glib::MainLoop::create();
  pipeline->set_state(Gst::STATE_PLAYING);
  std::cout << "Pipeline is playing." << std::endl;
  if(!have_type)
    mainloop->run();

  // Clean up nicely:
  std::cout << "Returned. Stopping pipeline." << std::endl;
  pipeline->set_state(Gst::STATE_NULL);

  return 0;
}
