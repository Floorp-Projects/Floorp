// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_RENDER_MESSAGES_H_
#define CHROME_COMMON_RENDER_MESSAGES_H_

#include <string>
#include <vector>
#include <map>

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "chrome/browser/renderer_host/resource_handler.h"
#include "chrome/common/filter_policy.h"
#include "chrome/common/ipc_message_utils.h"
#include "chrome/common/modal_dialog_event.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/transport_dib.h"
#include "chrome/common/webkit_param_traits.h"
#include "googleurl/src/gurl.h"
#include "media/audio/audio_output.h"
#include "net/base/upload_data.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_status.h"
#include "webkit/glue/autofill_form.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/password_form_dom_manager.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/glue/webaccessibility.h"
#include "webkit/glue/webappcachecontext.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/webplugininfo.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webview_delegate.h"

#if defined(OS_POSIX)
#include "skia/include/SkBitmap.h"
#endif

namespace base {
class Time;
}

// Parameters structure for ViewMsg_Navigate, which has too many data
// parameters to be reasonably put in a predefined IPC message.
struct ViewMsg_Navigate_Params {
  // The page_id for this navigation, or -1 if it is a new navigation.  Back,
  // Forward, and Reload navigations should have a valid page_id.  If the load
  // succeeds, then this page_id will be reflected in the resultant
  // ViewHostMsg_FrameNavigate message.
  int32_t page_id;

  // The URL to load.
  GURL url;

  // The URL to send in the "Referer" header field. Can be empty if there is
  // no referrer.
  GURL referrer;

  // The type of transition.
  PageTransition::Type transition;

  // Opaque history state (received by ViewHostMsg_UpdateState).
  std::string state;

  // Specifies if the URL should be loaded using 'reload' semantics (i.e.,
  // bypassing any locally cached content).
  bool reload;

  // The time the request was created
  base::Time request_time;
};

// Parameters structure for ViewHostMsg_FrameNavigate, which has too many data
// parameters to be reasonably put in a predefined IPC message.
struct ViewHostMsg_FrameNavigate_Params {
  // Page ID of this navigation. The renderer creates a new unique page ID
  // anytime a new session history entry is created. This means you'll get new
  // page IDs for user actions, and the old page IDs will be reloaded when
  // iframes are loaded automatically.
  int32_t page_id;

  // URL of the page being loaded.
  GURL url;

  // URL of the referrer of this load. WebKit generates this based on the
  // source of the event that caused the load.
  GURL referrer;

  // The type of transition.
  PageTransition::Type transition;

  // Lists the redirects that occurred on the way to the current page. This
  // vector has the same format as reported by the WebDataSource in the glue,
  // with the current page being the last one in the list (so even when
  // there's no redirect, there will be one entry in the list.
  std::vector<GURL> redirects;

  // Set to false if we want to update the session history but not update
  // the browser history.  E.g., on unreachable urls.
  bool should_update_history;

  // See SearchableFormData for a description of these.
  GURL searchable_form_url;
  std::wstring searchable_form_element_name;
  std::string searchable_form_encoding;

  // See password_form.h.
  PasswordForm password_form;

  // Information regarding the security of the connection (empty if the
  // connection was not secure).
  std::string security_info;

  // The gesture that initiated this navigation.
  NavigationGesture gesture;

  // Contents MIME type of main frame.
  std::string contents_mime_type;

  // True if this was a post request.
  bool is_post;

  // Whether the content of the frame was replaced with some alternate content
  // (this can happen if the resource was insecure).
  bool is_content_filtered;

  // The status code of the HTTP request.
  int http_status_code;
};

// Values that may be OR'd together to form the 'flags' parameter of a
// ViewHostMsg_PaintRect message.
struct ViewHostMsg_PaintRect_Flags {
  enum {
    IS_RESIZE_ACK = 1 << 0,
    IS_RESTORE_ACK = 1 << 1,
    IS_REPAINT_ACK = 1 << 2,
  };
  static bool is_resize_ack(int flags) {
    return (flags & IS_RESIZE_ACK) != 0;
  }
  static bool is_restore_ack(int flags) {
    return (flags & IS_RESTORE_ACK) != 0;
  }

  static bool is_repaint_ack(int flags) {
    return (flags & IS_REPAINT_ACK) != 0;
  }
};

struct ViewHostMsg_PaintRect_Params {
  // The bitmap to be painted into the rect given by bitmap_rect.
  TransportDIB::Id bitmap;

  // The position and size of the bitmap.
  gfx::Rect bitmap_rect;

  // The size of the RenderView when this message was generated.  This is
  // included so the host knows how large the view is from the perspective of
  // the renderer process.  This is necessary in case a resize operation is in
  // progress.
  gfx::Size view_size;

  // New window locations for plugin child windows.
  std::vector<WebPluginGeometry> plugin_window_moves;

  // The following describes the various bits that may be set in flags:
  //
  //   ViewHostMsg_PaintRect_Flags::IS_RESIZE_ACK
  //     Indicates that this is a response to a ViewMsg_Resize message.
  //
  //   ViewHostMsg_PaintRect_Flags::IS_RESTORE_ACK
  //     Indicates that this is a response to a ViewMsg_WasRestored message.
  //
  // If flags is zero, then this message corresponds to an unsoliticed paint
  // request by the render view.  Both of the above bits may be set in flags,
  // which would indicate that this paint message is an ACK for multiple
  // request messages.
  int flags;
};

// Parameters structure for ViewHostMsg_ScrollRect, which has too many data
// parameters to be reasonably put in a predefined IPC message.
struct ViewHostMsg_ScrollRect_Params {
  // The bitmap to be painted into the rect exposed by scrolling.
  TransportDIB::Id bitmap;

  // The position and size of the bitmap.
  gfx::Rect bitmap_rect;

  // The scroll offset.  Only one of these can be non-zero.
  int dx;
  int dy;

  // The rectangular region to scroll.
  gfx::Rect clip_rect;

  // The size of the RenderView when this message was generated.
  gfx::Size view_size;

  // New window locations for plugin child windows.
  std::vector<WebPluginGeometry> plugin_window_moves;
};

// Parameters structure for ViewMsg_UploadFile.
struct ViewMsg_UploadFile_Params {
  // See TabContents::StartFileUpload for a description of these fields.
  std::wstring file_path;
  std::wstring form;
  std::wstring file;
  std::wstring submit;
  std::wstring other_values;
};

// Parameters for a resource request.
struct ViewHostMsg_Resource_Request {
  // The request method: GET, POST, etc.
  std::string method;

  // The requested URL.
  GURL url;

  // The URL of the document in the top-level window, which may be checked by
  // the third-party cookie blocking policy. Leaving it empty may lead to
  // undesired cookie blocking. Third-party cookie blocking can be bypassed by
  // setting policy_url = url, but this should ideally only be done if there
  // really is no way to determine the correct value.
  GURL policy_url;

  // The referrer to use (may be empty).
  GURL referrer;

  // The origin of the frame that is associated with this request.  This is used
  // to update our mixed content state.
  std::string frame_origin;

  // The origin of the main frame (top-level frame) that is associated with this
  // request.  This is used to update our mixed content state.
  std::string main_frame_origin;

  // Additional HTTP request headers.
  std::string headers;

  // URLRequest load flags (0 by default).
  int load_flags;

  // Process ID of process that originated this request.
  int origin_pid;

  // What this resource load is for (main frame, sub-frame, sub-resource,
  // object).
  ResourceType::Type resource_type;

  // Used by plugin->browser requests to get the correct URLRequestContext.
  uint32_t request_context;

  // Indicates which frame (or worker context) the request is being loaded into,
  // or kNoAppCacheContextId.
  int32_t app_cache_context_id;

  // Optional upload data (may be null).
  scoped_refptr<net::UploadData> upload_data;
};

// Parameters for a render request.
struct ViewMsg_Print_Params {
  // In pixels according to dpi_x and dpi_y.
  gfx::Size printable_size;

  // Specifies dots per inch.
  double dpi;

  // Minimum shrink factor. See PrintSettings::min_shrink for more information.
  double min_shrink;

  // Maximum shrink factor. See PrintSettings::max_shrink for more information.
  double max_shrink;

  // Desired apparent dpi on paper.
  int desired_dpi;

  // Cookie for the document to ensure correctness.
  int document_cookie;

  // Warning: do not compare document_cookie.
  bool Equals(const ViewMsg_Print_Params& rhs) const {
    return printable_size == rhs.printable_size &&
           dpi == rhs.dpi &&
           min_shrink == rhs.min_shrink &&
           max_shrink == rhs.max_shrink &&
           desired_dpi == rhs.desired_dpi;
  }

  // Checking if the current params is empty. Just initialized after a memset.
  bool IsEmpty() const {
    return !document_cookie && !desired_dpi && !max_shrink && !min_shrink &&
           !dpi && printable_size.IsEmpty();
  }
};

struct ViewMsg_PrintPage_Params {
  // Parameters to render the page as a printed page. It must always be the same
  // value for all the document.
  ViewMsg_Print_Params params;

  // The page number is the indicator of the square that should be rendered
  // according to the layout specified in ViewMsg_Print_Params.
  int page_number;
};

struct ViewMsg_PrintPages_Params {
  // Parameters to render the page as a printed page. It must always be the same
  // value for all the document.
  ViewMsg_Print_Params params;

  // If empty, this means a request to render all the printed pages.
  std::vector<int> pages;
};

// Parameters to describe a rendered page.
struct ViewHostMsg_DidPrintPage_Params {
  // A shared memory handle to the EMF data. This data can be quite large so a
  // memory map needs to be used.
  base::SharedMemoryHandle emf_data_handle;

  // Size of the EMF data.
  unsigned data_size;

  // Cookie for the document to ensure correctness.
  int document_cookie;

  // Page number.
  int page_number;

  // Shrink factor used to render this page.
  double actual_shrink;
};

// The first parameter for the ViewHostMsg_ImeUpdateStatus message.
enum ViewHostMsg_ImeControl {
  IME_DISABLE = 0,
  IME_MOVE_WINDOWS,
  IME_COMPLETE_COMPOSITION,
};

// Parameters for creating an audio output stream.
struct ViewHostMsg_Audio_CreateStream {
  // Format request for the stream.
  AudioManager::Format format;

  // Number of channels.
  int channels;

  // Sampling rate (frequency) of the output stream.
  int sample_rate;

  // Number of bits per sample;
  int bits_per_sample;

  // Number of bytes per packet.
  size_t packet_size;
};

// This message is used for supporting popup menus on Mac OS X using native
// Cocoa controls. The renderer sends us this message which we use to populate
// the popup menu.
struct ViewHostMsg_ShowPopup_Params {
  // Position on the screen.
  gfx::Rect bounds;

  // The height of each item in the menu.
  int item_height;

  // The currently selected (displayed) item in the menu.
  int selected_item;

  // The entire list of items in the popup menu.
  std::vector<WebMenuItem> popup_items;
};

namespace IPC {

template <>
struct ParamTraits<ResourceType::Type> {
  typedef ResourceType::Type param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type) || !ResourceType::ValidType(type))
      return false;
    *p = ResourceType::FromInt(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring type;
    switch (p) {
      case ResourceType::MAIN_FRAME:
        type = L"MAIN_FRAME";
       break;
     case ResourceType::SUB_FRAME:
       type = L"SUB_FRAME";
       break;
     case ResourceType::SUB_RESOURCE:
       type = L"SUB_RESOURCE";
       break;
     case ResourceType::OBJECT:
       type = L"OBJECT";
       break;
     case ResourceType::MEDIA:
       type = L"MEDIA";
       break;
     default:
       type = L"UNKNOWN";
       break;
    }

    LogParam(type, l);
  }
};

template <>
struct ParamTraits<FilterPolicy::Type> {
  typedef FilterPolicy::Type param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type) || !FilterPolicy::ValidType(type))
      return false;
    *p = FilterPolicy::FromInt(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring type;
    switch (p) {
      case FilterPolicy::DONT_FILTER:
        type = L"DONT_FILTER";
        break;
      case FilterPolicy::FILTER_ALL:
        type = L"FILTER_ALL";
        break;
      case FilterPolicy::FILTER_ALL_EXCEPT_IMAGES:
        type = L"FILTER_ALL_EXCEPT_IMAGES";
        break;
      default:
        type = L"UNKNOWN";
        break;
    }

    LogParam(type, l);
  }
};

template <>
struct ParamTraits<ContextNode> {
  typedef ContextNode param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p.type);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = ContextNode(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring event = L"";

    if (!p.type) {
      event.append(L"NONE");
    } else {
      event.append(L"(");
      if (p.type & ContextNode::PAGE)
        event.append(L"PAGE|");
      if (p.type & ContextNode::FRAME)
        event.append(L"FRAME|");
      if (p.type & ContextNode::LINK)
        event.append(L"LINK|");
      if (p.type & ContextNode::IMAGE)
        event.append(L"IMAGE|");
      if (p.type & ContextNode::SELECTION)
        event.append(L"SELECTION|");
      if (p.type & ContextNode::EDITABLE)
        event.append(L"EDITABLE|");
      if (p.type & ContextNode::MISSPELLED_WORD)
        event.append(L"MISSPELLED_WORD|");
      event.append(L")");
    }

    LogParam(event, l);
  }
};

template <>
struct ParamTraits<webkit_glue::WebAccessibility::InParams> {
  typedef webkit_glue::WebAccessibility::InParams param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.object_id);
    WriteParam(m, p.function_id);
    WriteParam(m, p.child_id);
    WriteParam(m, p.input_long1);
    WriteParam(m, p.input_long2);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->object_id) &&
      ReadParam(m, iter, &p->function_id) &&
      ReadParam(m, iter, &p->child_id) &&
      ReadParam(m, iter, &p->input_long1) &&
      ReadParam(m, iter, &p->input_long2);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.object_id, l);
    l->append(L", ");
    LogParam(p.function_id, l);
    l->append(L", ");
    LogParam(p.child_id, l);
    l->append(L", ");
    LogParam(p.input_long1, l);
    l->append(L", ");
    LogParam(p.input_long2, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<webkit_glue::WebAccessibility::OutParams> {
  typedef webkit_glue::WebAccessibility::OutParams param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.object_id);
    WriteParam(m, p.output_long1);
    WriteParam(m, p.output_long2);
    WriteParam(m, p.output_long3);
    WriteParam(m, p.output_long4);
    WriteParam(m, p.output_string);
    WriteParam(m, p.return_code);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->object_id) &&
      ReadParam(m, iter, &p->output_long1) &&
      ReadParam(m, iter, &p->output_long2) &&
      ReadParam(m, iter, &p->output_long3) &&
      ReadParam(m, iter, &p->output_long4) &&
      ReadParam(m, iter, &p->output_string) &&
      ReadParam(m, iter, &p->return_code);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.object_id, l);
    l->append(L", ");
    LogParam(p.output_long1, l);
    l->append(L", ");
    LogParam(p.output_long2, l);
    l->append(L", ");
    LogParam(p.output_long3, l);
    l->append(L", ");
    LogParam(p.output_long4, l);
    l->append(L", ");
    LogParam(p.output_string, l);
    l->append(L", ");
    LogParam(p.return_code, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<ViewHostMsg_ImeControl> {
  typedef ViewHostMsg_ImeControl param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<ViewHostMsg_ImeControl>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring control;
    switch (p) {
     case IME_DISABLE:
      control = L"IME_DISABLE";
      break;
     case IME_MOVE_WINDOWS:
      control = L"IME_MOVE_WINDOWS";
      break;
     case IME_COMPLETE_COMPOSITION:
      control = L"IME_COMPLETE_COMPOSITION";
      break;
     default:
      control = L"UNKNOWN";
      break;
    }

    LogParam(control, l);
  }
};

// Traits for ViewMsg_Navigate_Params structure to pack/unpack.
template <>
struct ParamTraits<ViewMsg_Navigate_Params> {
  typedef ViewMsg_Navigate_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.page_id);
    WriteParam(m, p.url);
    WriteParam(m, p.referrer);
    WriteParam(m, p.transition);
    WriteParam(m, p.state);
    WriteParam(m, p.reload);
    WriteParam(m, p.request_time);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->page_id) &&
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->referrer) &&
      ReadParam(m, iter, &p->transition) &&
      ReadParam(m, iter, &p->state) &&
      ReadParam(m, iter, &p->reload) &&
      ReadParam(m, iter, &p->request_time);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.page_id, l);
    l->append(L", ");
    LogParam(p.url, l);
    l->append(L", ");
    LogParam(p.transition, l);
    l->append(L", ");
    LogParam(p.state, l);
    l->append(L", ");
    LogParam(p.reload, l);
    l->append(L", ");
    LogParam(p.request_time, l);
    l->append(L")");
  }
};

// Traits for PasswordForm_Params structure to pack/unpack.
template <>
struct ParamTraits<PasswordForm> {
  typedef PasswordForm param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.signon_realm);
    WriteParam(m, p.origin);
    WriteParam(m, p.action);
    WriteParam(m, p.submit_element);
    WriteParam(m, p.username_element);
    WriteParam(m, p.username_value);
    WriteParam(m, p.password_element);
    WriteParam(m, p.password_value);
    WriteParam(m, p.old_password_element);
    WriteParam(m, p.old_password_value);
    WriteParam(m, p.ssl_valid);
    WriteParam(m, p.preferred);
    WriteParam(m, p.blacklisted_by_user);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->signon_realm) &&
      ReadParam(m, iter, &p->origin) &&
      ReadParam(m, iter, &p->action) &&
      ReadParam(m, iter, &p->submit_element) &&
      ReadParam(m, iter, &p->username_element) &&
      ReadParam(m, iter, &p->username_value) &&
      ReadParam(m, iter, &p->password_element) &&
      ReadParam(m, iter, &p->password_value) &&
      ReadParam(m, iter, &p->old_password_element) &&
      ReadParam(m, iter, &p->old_password_value) &&
      ReadParam(m, iter, &p->ssl_valid) &&
      ReadParam(m, iter, &p->preferred) &&
      ReadParam(m, iter, &p->blacklisted_by_user);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<PasswordForm>");
  }
};

// Traits for AutofillForm_Params structure to pack/unpack.
template <>
struct ParamTraits<AutofillForm> {
  typedef AutofillForm param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.elements.size());
    for (std::vector<AutofillForm::Element>::const_iterator itr =
        p.elements.begin();
        itr != p.elements.end();
        itr++) {
      WriteParam(m, itr->name);
      WriteParam(m, itr->value);
    }
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
      bool result = true;
      size_t elements_size = 0;
      result = result && ReadParam(m, iter, &elements_size);
      p->elements.resize(elements_size);
      for (size_t i = 0; i < elements_size; i++) {
        result = result && ReadParam(m, iter, &(p->elements[i].name));
        result = result && ReadParam(m, iter, &(p->elements[i].value));
      }
      return result;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<AutofillForm>");
  }
};

// Traits for ViewHostMsg_FrameNavigate_Params structure to pack/unpack.
template <>
struct ParamTraits<ViewHostMsg_FrameNavigate_Params> {
  typedef ViewHostMsg_FrameNavigate_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.page_id);
    WriteParam(m, p.url);
    WriteParam(m, p.referrer);
    WriteParam(m, p.transition);
    WriteParam(m, p.redirects);
    WriteParam(m, p.should_update_history);
    WriteParam(m, p.searchable_form_url);
    WriteParam(m, p.searchable_form_element_name);
    WriteParam(m, p.searchable_form_encoding);
    WriteParam(m, p.password_form);
    WriteParam(m, p.security_info);
    WriteParam(m, p.gesture);
    WriteParam(m, p.contents_mime_type);
    WriteParam(m, p.is_post);
    WriteParam(m, p.is_content_filtered);
    WriteParam(m, p.http_status_code);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->page_id) &&
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->referrer) &&
      ReadParam(m, iter, &p->transition) &&
      ReadParam(m, iter, &p->redirects) &&
      ReadParam(m, iter, &p->should_update_history) &&
      ReadParam(m, iter, &p->searchable_form_url) &&
      ReadParam(m, iter, &p->searchable_form_element_name) &&
      ReadParam(m, iter, &p->searchable_form_encoding) &&
      ReadParam(m, iter, &p->password_form) &&
      ReadParam(m, iter, &p->security_info) &&
      ReadParam(m, iter, &p->gesture) &&
      ReadParam(m, iter, &p->contents_mime_type) &&
      ReadParam(m, iter, &p->is_post) &&
      ReadParam(m, iter, &p->is_content_filtered) &&
      ReadParam(m, iter, &p->http_status_code);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.page_id, l);
    l->append(L", ");
    LogParam(p.url, l);
    l->append(L", ");
    LogParam(p.referrer, l);
    l->append(L", ");
    LogParam(p.transition, l);
    l->append(L", ");
    LogParam(p.redirects, l);
    l->append(L", ");
    LogParam(p.should_update_history, l);
    l->append(L", ");
    LogParam(p.searchable_form_url, l);
    l->append(L", ");
    LogParam(p.searchable_form_element_name, l);
    l->append(L", ");
    LogParam(p.searchable_form_encoding, l);
    l->append(L", ");
    LogParam(p.password_form, l);
    l->append(L", ");
    LogParam(p.security_info, l);
    l->append(L", ");
    LogParam(p.gesture, l);
    l->append(L", ");
    LogParam(p.contents_mime_type, l);
    l->append(L", ");
    LogParam(p.is_post, l);
    l->append(L", ");
    LogParam(p.is_content_filtered, l);
    l->append(L", ");
    LogParam(p.http_status_code, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<ContextMenuParams> {
  typedef ContextMenuParams param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.node);
    WriteParam(m, p.x);
    WriteParam(m, p.y);
    WriteParam(m, p.link_url);
    WriteParam(m, p.unfiltered_link_url);
    WriteParam(m, p.image_url);
    WriteParam(m, p.page_url);
    WriteParam(m, p.frame_url);
    WriteParam(m, p.selection_text);
    WriteParam(m, p.misspelled_word);
    WriteParam(m, p.dictionary_suggestions);
    WriteParam(m, p.spellcheck_enabled);
    WriteParam(m, p.edit_flags);
    WriteParam(m, p.security_info);
    WriteParam(m, p.frame_charset);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->node) &&
      ReadParam(m, iter, &p->x) &&
      ReadParam(m, iter, &p->y) &&
      ReadParam(m, iter, &p->link_url) &&
      ReadParam(m, iter, &p->unfiltered_link_url) &&
      ReadParam(m, iter, &p->image_url) &&
      ReadParam(m, iter, &p->page_url) &&
      ReadParam(m, iter, &p->frame_url) &&
      ReadParam(m, iter, &p->selection_text) &&
      ReadParam(m, iter, &p->misspelled_word) &&
      ReadParam(m, iter, &p->dictionary_suggestions) &&
      ReadParam(m, iter, &p->spellcheck_enabled) &&
      ReadParam(m, iter, &p->edit_flags) &&
      ReadParam(m, iter, &p->security_info) &&
      ReadParam(m, iter, &p->frame_charset);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ContextMenuParams>");
  }
};

// Traits for ViewHostMsg_PaintRect_Params structure to pack/unpack.
template <>
struct ParamTraits<ViewHostMsg_PaintRect_Params> {
  typedef ViewHostMsg_PaintRect_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.bitmap);
    WriteParam(m, p.bitmap_rect);
    WriteParam(m, p.view_size);
    WriteParam(m, p.plugin_window_moves);
    WriteParam(m, p.flags);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->bitmap) &&
      ReadParam(m, iter, &p->bitmap_rect) &&
      ReadParam(m, iter, &p->view_size) &&
      ReadParam(m, iter, &p->plugin_window_moves) &&
      ReadParam(m, iter, &p->flags);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.bitmap, l);
    l->append(L", ");
    LogParam(p.bitmap_rect, l);
    l->append(L", ");
    LogParam(p.view_size, l);
    l->append(L", ");
    LogParam(p.plugin_window_moves, l);
    l->append(L", ");
    LogParam(p.flags, l);
    l->append(L")");
  }
};

// Traits for ViewHostMsg_ScrollRect_Params structure to pack/unpack.
template <>
struct ParamTraits<ViewHostMsg_ScrollRect_Params> {
  typedef ViewHostMsg_ScrollRect_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.bitmap);
    WriteParam(m, p.bitmap_rect);
    WriteParam(m, p.dx);
    WriteParam(m, p.dy);
    WriteParam(m, p.clip_rect);
    WriteParam(m, p.view_size);
    WriteParam(m, p.plugin_window_moves);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->bitmap) &&
      ReadParam(m, iter, &p->bitmap_rect) &&
      ReadParam(m, iter, &p->dx) &&
      ReadParam(m, iter, &p->dy) &&
      ReadParam(m, iter, &p->clip_rect) &&
      ReadParam(m, iter, &p->view_size) &&
      ReadParam(m, iter, &p->plugin_window_moves);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.bitmap, l);
    l->append(L", ");
    LogParam(p.bitmap_rect, l);
    l->append(L", ");
    LogParam(p.dx, l);
    l->append(L", ");
    LogParam(p.dy, l);
    l->append(L", ");
    LogParam(p.clip_rect, l);
    l->append(L", ");
    LogParam(p.view_size, l);
    l->append(L", ");
    LogParam(p.plugin_window_moves, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<WebPluginGeometry> {
  typedef WebPluginGeometry param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.window);
    WriteParam(m, p.window_rect);
    WriteParam(m, p.clip_rect);
    WriteParam(m, p.cutout_rects);
    WriteParam(m, p.visible);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->window) &&
      ReadParam(m, iter, &p->window_rect) &&
      ReadParam(m, iter, &p->clip_rect) &&
      ReadParam(m, iter, &p->cutout_rects) &&
      ReadParam(m, iter, &p->visible);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.window, l);
    l->append(L", ");
    LogParam(p.window_rect, l);
    l->append(L", ");
    LogParam(p.clip_rect, l);
    l->append(L", ");
    LogParam(p.cutout_rects, l);
    l->append(L", ");
    LogParam(p.visible, l);
    l->append(L")");
  }
};

// Traits for ViewMsg_GetPlugins_Reply structure to pack/unpack.
template <>
struct ParamTraits<WebPluginMimeType> {
  typedef WebPluginMimeType param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.mime_type);
    WriteParam(m, p.file_extensions);
    WriteParam(m, p.description);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->mime_type) &&
      ReadParam(m, iter, &r->file_extensions) &&
      ReadParam(m, iter, &r->description);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.mime_type, l);
    l->append(L", ");
    LogParam(p.file_extensions, l);
    l->append(L", ");
    LogParam(p.description, l);
    l->append(L")");
  }
};


template <>
struct ParamTraits<WebPluginInfo> {
  typedef WebPluginInfo param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.name);
    WriteParam(m, p.path);
    WriteParam(m, p.version);
    WriteParam(m, p.desc);
    WriteParam(m, p.mime_types);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->name) &&
      ReadParam(m, iter, &r->path) &&
      ReadParam(m, iter, &r->version) &&
      ReadParam(m, iter, &r->desc) &&
      ReadParam(m, iter, &r->mime_types);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.name, l);
    l->append(L", ");
    l->append(L", ");
    LogParam(p.path, l);
    l->append(L", ");
    LogParam(p.version, l);
    l->append(L", ");
    LogParam(p.desc, l);
    l->append(L", ");
    LogParam(p.mime_types, l);
    l->append(L")");
  }
};

// Traits for ViewMsg_UploadFile_Params structure to pack/unpack.
template <>
struct ParamTraits<ViewMsg_UploadFile_Params> {
  typedef ViewMsg_UploadFile_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.file_path);
    WriteParam(m, p.form);
    WriteParam(m, p.file);
    WriteParam(m, p.submit);
    WriteParam(m, p.other_values);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->file_path) &&
      ReadParam(m, iter, &p->form) &&
      ReadParam(m, iter, &p->file) &&
      ReadParam(m, iter, &p->submit) &&
      ReadParam(m, iter, &p->other_values);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ViewMsg_UploadFile_Params>");
  }
};

// Traits for net::UploadData::Element.
template <>
struct ParamTraits<net::UploadData::Element> {
  typedef net::UploadData::Element param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.type()));
    if (p.type() == net::UploadData::TYPE_BYTES) {
      m->WriteData(&p.bytes()[0], static_cast<int>(p.bytes().size()));
    } else {
      WriteParam(m, p.file_path());
      WriteParam(m, p.file_range_offset());
      WriteParam(m, p.file_range_length());
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int type;
    if (!ReadParam(m, iter, &type))
      return false;
    if (type == net::UploadData::TYPE_BYTES) {
      const char* data;
      int len;
      if (!m->ReadData(iter, &data, &len))
        return false;
      r->SetToBytes(data, len);
    } else {
      DCHECK(type == net::UploadData::TYPE_FILE);
      FilePath file_path;
      uint64_t offset, length;
      if (!ReadParam(m, iter, &file_path))
        return false;
      if (!ReadParam(m, iter, &offset))
        return false;
      if (!ReadParam(m, iter, &length))
        return false;
      r->SetToFilePathRange(file_path, offset, length);
    }
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<net::UploadData::Element>");
  }
};

// Traits for net::UploadData.
template <>
struct ParamTraits<scoped_refptr<net::UploadData> > {
  typedef scoped_refptr<net::UploadData> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.get() != NULL);
    if (p) {
      WriteParam(m, p->elements());
      WriteParam(m, p->identifier());
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    bool has_object;
    if (!ReadParam(m, iter, &has_object))
      return false;
    if (!has_object)
      return true;
    std::vector<net::UploadData::Element> elements;
    if (!ReadParam(m, iter, &elements))
      return false;
    int identifier;
    if (!ReadParam(m, iter, &identifier))
      return false;
    *r = new net::UploadData;
    (*r)->swap_elements(&elements);
    (*r)->set_identifier(identifier);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<net::UploadData>");
  }
};

// Traits for PasswordFormDomManager::FillData.
template <>
struct ParamTraits<PasswordFormDomManager::FillData> {
  typedef PasswordFormDomManager::FillData param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.basic_data);
    WriteParam(m, p.additional_logins);
    WriteParam(m, p.wait_for_username);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->basic_data) &&
      ReadParam(m, iter, &r->additional_logins) &&
      ReadParam(m, iter, &r->wait_for_username);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<PasswordFormDomManager::FillData>");
  }
};

template<>
struct ParamTraits<NavigationGesture> {
  typedef NavigationGesture param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<NavigationGesture>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring event;
    switch (p) {
      case NavigationGestureUser:
        event = L"GESTURE_USER";
        break;
      case NavigationGestureAuto:
        event = L"GESTURE_AUTO";
        break;
      default:
        event = L"GESTURE_UNKNOWN";
        break;
    }
    LogParam(event, l);
  }
};

// Traits for ViewHostMsg_Resource_Request
template <>
struct ParamTraits<ViewHostMsg_Resource_Request> {
  typedef ViewHostMsg_Resource_Request param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.method);
    WriteParam(m, p.url);
    WriteParam(m, p.policy_url);
    WriteParam(m, p.referrer);
    WriteParam(m, p.frame_origin);
    WriteParam(m, p.main_frame_origin);
    WriteParam(m, p.headers);
    WriteParam(m, p.load_flags);
    WriteParam(m, p.origin_pid);
    WriteParam(m, p.resource_type);
    WriteParam(m, p.request_context);
    WriteParam(m, p.app_cache_context_id);
    WriteParam(m, p.upload_data);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->method) &&
      ReadParam(m, iter, &r->url) &&
      ReadParam(m, iter, &r->policy_url) &&
      ReadParam(m, iter, &r->referrer) &&
      ReadParam(m, iter, &r->frame_origin) &&
      ReadParam(m, iter, &r->main_frame_origin) &&
      ReadParam(m, iter, &r->headers) &&
      ReadParam(m, iter, &r->load_flags) &&
      ReadParam(m, iter, &r->origin_pid) &&
      ReadParam(m, iter, &r->resource_type) &&
      ReadParam(m, iter, &r->request_context) &&
      ReadParam(m, iter, &r->app_cache_context_id) &&
      ReadParam(m, iter, &r->upload_data);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.method, l);
    l->append(L", ");
    LogParam(p.url, l);
    l->append(L", ");
    LogParam(p.referrer, l);
    l->append(L", ");
    LogParam(p.frame_origin, l);
    l->append(L", ");
    LogParam(p.main_frame_origin, l);
    l->append(L", ");
    LogParam(p.load_flags, l);
    l->append(L", ");
    LogParam(p.origin_pid, l);
    l->append(L", ");
    LogParam(p.resource_type, l);
    l->append(L", ");
    LogParam(p.request_context, l);
    l->append(L", ");
    LogParam(p.app_cache_context_id, l);
    l->append(L")");
  }
};

// Traits for URLRequestStatus
template <>
struct ParamTraits<URLRequestStatus> {
  typedef URLRequestStatus param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.status()));
    WriteParam(m, p.os_error());
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int status, os_error;
    if (!ReadParam(m, iter, &status) ||
        !ReadParam(m, iter, &os_error))
      return false;
    r->set_status(static_cast<URLRequestStatus::Status>(status));
    r->set_os_error(os_error);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring status;
    switch (p.status()) {
     case URLRequestStatus::SUCCESS:
      status = L"SUCCESS";
      break;
     case URLRequestStatus::IO_PENDING:
      status = L"IO_PENDING ";
      break;
     case URLRequestStatus::HANDLED_EXTERNALLY:
      status = L"HANDLED_EXTERNALLY";
      break;
     case URLRequestStatus::CANCELED:
      status = L"CANCELED";
      break;
     case URLRequestStatus::FAILED:
      status = L"FAILED";
      break;
     default:
      status = L"UNKNOWN";
      break;
    }
    if (p.status() == URLRequestStatus::FAILED)
      l->append(L"(");

    LogParam(status, l);

    if (p.status() == URLRequestStatus::FAILED) {
      l->append(L", ");
      LogParam(p.os_error(), l);
      l->append(L")");
    }
  }
};

template <>
struct ParamTraits<scoped_refptr<net::HttpResponseHeaders> > {
  typedef scoped_refptr<net::HttpResponseHeaders> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.get() != NULL);
    if (p) {
      // Do not disclose Set-Cookie headers over IPC.
      p->Persist(m, net::HttpResponseHeaders::PERSIST_SANS_COOKIES);
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    bool has_object;
    if (!ReadParam(m, iter, &has_object))
      return false;
    if (has_object)
      *r = new net::HttpResponseHeaders(*m, iter);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<HttpResponseHeaders>");
  }
};

// Traits for webkit_glue::ResourceLoaderBridge::ResponseInfo
template <>
struct ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo> {
  typedef webkit_glue::ResourceLoaderBridge::ResponseInfo param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.request_time);
    WriteParam(m, p.response_time);
    WriteParam(m, p.headers);
    WriteParam(m, p.mime_type);
    WriteParam(m, p.charset);
    WriteParam(m, p.security_info);
    WriteParam(m, p.content_length);
    WriteParam(m, p.app_cache_id);
    WriteParam(m, p.response_data_file);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->request_time) &&
      ReadParam(m, iter, &r->response_time) &&
      ReadParam(m, iter, &r->headers) &&
      ReadParam(m, iter, &r->mime_type) &&
      ReadParam(m, iter, &r->charset) &&
      ReadParam(m, iter, &r->security_info) &&
      ReadParam(m, iter, &r->content_length) &&
      ReadParam(m, iter, &r->app_cache_id) &&
      ReadParam(m, iter, &r->response_data_file);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.request_time, l);
    l->append(L", ");
    LogParam(p.response_time, l);
    l->append(L", ");
    LogParam(p.headers, l);
    l->append(L", ");
    LogParam(p.mime_type, l);
    l->append(L", ");
    LogParam(p.charset, l);
    l->append(L", ");
    LogParam(p.security_info, l);
    l->append(L", ");
    LogParam(p.content_length, l);
    l->append(L", ");
    LogParam(p.app_cache_id, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<ResourceResponseHead> {
  typedef ResourceResponseHead param_type;
  static void Write(Message* m, const param_type& p) {
    ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo>::Write(m, p);
    WriteParam(m, p.status);
    WriteParam(m, p.filter_policy);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo>::Read(m,
                                                                         iter,
                                                                         r) &&
      ReadParam(m, iter, &r->status) &&
      ReadParam(m, iter, &r->filter_policy);
  }
  static void Log(const param_type& p, std::wstring* l) {
    // log more?
    ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo>::Log(p, l);
  }
};

template <>
struct ParamTraits<SyncLoadResult> {
  typedef SyncLoadResult param_type;
  static void Write(Message* m, const param_type& p) {
    ParamTraits<ResourceResponseHead>::Write(m, p);
    WriteParam(m, p.final_url);
    WriteParam(m, p.data);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ParamTraits<ResourceResponseHead>::Read(m, iter, r) &&
      ReadParam(m, iter, &r->final_url) &&
      ReadParam(m, iter, &r->data);
  }
  static void Log(const param_type& p, std::wstring* l) {
    // log more?
    ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo>::Log(p, l);
  }
};

// Traits for FormData structure to pack/unpack.
template <>
struct ParamTraits<FormData> {
  typedef FormData param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.origin);
    WriteParam(m, p.action);
    WriteParam(m, p.elements);
    WriteParam(m, p.values);
    WriteParam(m, p.submit);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->origin) &&
      ReadParam(m, iter, &p->action) &&
      ReadParam(m, iter, &p->elements) &&
      ReadParam(m, iter, &p->values) &&
      ReadParam(m, iter, &p->submit);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<FormData>");
  }
};

// Traits for ViewMsg_Print_Params
template <>
struct ParamTraits<ViewMsg_Print_Params> {
  typedef ViewMsg_Print_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.printable_size);
    WriteParam(m, p.dpi);
    WriteParam(m, p.min_shrink);
    WriteParam(m, p.max_shrink);
    WriteParam(m, p.desired_dpi);
    WriteParam(m, p.document_cookie);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->printable_size) &&
           ReadParam(m, iter, &p->dpi) &&
           ReadParam(m, iter, &p->min_shrink) &&
           ReadParam(m, iter, &p->max_shrink) &&
           ReadParam(m, iter, &p->desired_dpi) &&
           ReadParam(m, iter, &p->document_cookie);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ViewMsg_Print_Params>");
  }
};

// Traits for ViewMsg_PrintPage_Params
template <>
struct ParamTraits<ViewMsg_PrintPage_Params> {
  typedef ViewMsg_PrintPage_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.params);
    WriteParam(m, p.page_number);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->params) &&
           ReadParam(m, iter, &p->page_number);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ViewMsg_PrintPage_Params>");
  }
};

// Traits for ViewMsg_PrintPages_Params
template <>
struct ParamTraits<ViewMsg_PrintPages_Params> {
  typedef ViewMsg_PrintPages_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.params);
    WriteParam(m, p.pages);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->params) &&
           ReadParam(m, iter, &p->pages);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ViewMsg_PrintPages_Params>");
  }
};

// Traits for ViewHostMsg_DidPrintPage_Params
template <>
struct ParamTraits<ViewHostMsg_DidPrintPage_Params> {
  typedef ViewHostMsg_DidPrintPage_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.emf_data_handle);
    WriteParam(m, p.data_size);
    WriteParam(m, p.document_cookie);
    WriteParam(m, p.page_number);
    WriteParam(m, p.actual_shrink);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->emf_data_handle) &&
           ReadParam(m, iter, &p->data_size) &&
           ReadParam(m, iter, &p->document_cookie) &&
           ReadParam(m, iter, &p->page_number) &&
           ReadParam(m, iter, &p->actual_shrink);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ViewHostMsg_DidPrintPage_Params>");
  }
};

// Traits for WebPreferences structure to pack/unpack.
template <>
struct ParamTraits<WebPreferences> {
  typedef WebPreferences param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.standard_font_family);
    WriteParam(m, p.fixed_font_family);
    WriteParam(m, p.serif_font_family);
    WriteParam(m, p.sans_serif_font_family);
    WriteParam(m, p.cursive_font_family);
    WriteParam(m, p.fantasy_font_family);
    WriteParam(m, p.default_font_size);
    WriteParam(m, p.default_fixed_font_size);
    WriteParam(m, p.minimum_font_size);
    WriteParam(m, p.minimum_logical_font_size);
    WriteParam(m, p.default_encoding);
    WriteParam(m, p.javascript_enabled);
    WriteParam(m, p.web_security_enabled);
    WriteParam(m, p.javascript_can_open_windows_automatically);
    WriteParam(m, p.loads_images_automatically);
    WriteParam(m, p.plugins_enabled);
    WriteParam(m, p.dom_paste_enabled);
    WriteParam(m, p.developer_extras_enabled);
    WriteParam(m, p.shrinks_standalone_images_to_fit);
    WriteParam(m, p.uses_universal_detector);
    WriteParam(m, p.text_areas_are_resizable);
    WriteParam(m, p.java_enabled);
    WriteParam(m, p.user_style_sheet_enabled);
    WriteParam(m, p.user_style_sheet_location);
    WriteParam(m, p.uses_page_cache);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
        ReadParam(m, iter, &p->standard_font_family) &&
        ReadParam(m, iter, &p->fixed_font_family) &&
        ReadParam(m, iter, &p->serif_font_family) &&
        ReadParam(m, iter, &p->sans_serif_font_family) &&
        ReadParam(m, iter, &p->cursive_font_family) &&
        ReadParam(m, iter, &p->fantasy_font_family) &&
        ReadParam(m, iter, &p->default_font_size) &&
        ReadParam(m, iter, &p->default_fixed_font_size) &&
        ReadParam(m, iter, &p->minimum_font_size) &&
        ReadParam(m, iter, &p->minimum_logical_font_size) &&
        ReadParam(m, iter, &p->default_encoding) &&
        ReadParam(m, iter, &p->javascript_enabled) &&
        ReadParam(m, iter, &p->web_security_enabled) &&
        ReadParam(m, iter, &p->javascript_can_open_windows_automatically) &&
        ReadParam(m, iter, &p->loads_images_automatically) &&
        ReadParam(m, iter, &p->plugins_enabled) &&
        ReadParam(m, iter, &p->dom_paste_enabled) &&
        ReadParam(m, iter, &p->developer_extras_enabled) &&
        ReadParam(m, iter, &p->shrinks_standalone_images_to_fit) &&
        ReadParam(m, iter, &p->uses_universal_detector) &&
        ReadParam(m, iter, &p->text_areas_are_resizable) &&
        ReadParam(m, iter, &p->java_enabled) &&
        ReadParam(m, iter, &p->user_style_sheet_enabled) &&
        ReadParam(m, iter, &p->user_style_sheet_location) &&
        ReadParam(m, iter, &p->uses_page_cache);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<WebPreferences>");
  }
};

// Traits for WebDropData
template <>
struct ParamTraits<WebDropData> {
  typedef WebDropData param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.identity);
    WriteParam(m, p.url);
    WriteParam(m, p.url_title);
    WriteParam(m, p.file_extension);
    WriteParam(m, p.filenames);
    WriteParam(m, p.plain_text);
    WriteParam(m, p.text_html);
    WriteParam(m, p.html_base_url);
    WriteParam(m, p.file_description_filename);
    WriteParam(m, p.file_contents);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->identity) &&
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->url_title) &&
      ReadParam(m, iter, &p->file_extension) &&
      ReadParam(m, iter, &p->filenames) &&
      ReadParam(m, iter, &p->plain_text) &&
      ReadParam(m, iter, &p->text_html) &&
      ReadParam(m, iter, &p->html_base_url) &&
      ReadParam(m, iter, &p->file_description_filename) &&
      ReadParam(m, iter, &p->file_contents);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<WebDropData>");
  }
};

template<>
struct ParamTraits<ModalDialogEvent> {
  typedef ModalDialogEvent param_type;
#if defined(OS_WIN)
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.event);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->event);
  }
#else
  static void Write(Message* m, const param_type& p) {
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return true;
  }
#endif

  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ModalDialogEvent>");
  }
};

// Traits for AudioManager::Format.
template <>
struct ParamTraits<AudioManager::Format> {
  typedef AudioManager::Format param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<AudioManager::Format>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring format;
    switch (p) {
     case AudioManager::AUDIO_PCM_LINEAR:
       format = L"AUDIO_PCM_LINEAR";
       break;
     case AudioManager::AUDIO_PCM_DELTA:
       format = L"AUDIO_PCM_DELTA";
       break;
     case AudioManager::AUDIO_MOCK:
       format = L"AUDIO_MOCK";
       break;
     default:
       format = L"AUDIO_LAST_FORMAT";
       break;
    }
    LogParam(format, l);
  }
};

// Traits for ViewHostMsg_Audio_CreateStream.
template <>
struct ParamTraits<ViewHostMsg_Audio_CreateStream> {
  typedef ViewHostMsg_Audio_CreateStream param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.format);
    WriteParam(m, p.channels);
    WriteParam(m, p.sample_rate);
    WriteParam(m, p.bits_per_sample);
    WriteParam(m, p.packet_size);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->format) &&
      ReadParam(m, iter, &p->channels) &&
      ReadParam(m, iter, &p->sample_rate) &&
      ReadParam(m, iter, &p->bits_per_sample) &&
      ReadParam(m, iter, &p->packet_size);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ViewHostMsg_Audio_CreateStream>(");
    LogParam(p.format, l);
    l->append(L", ");
    LogParam(p.channels, l);
    l->append(L", ");
    LogParam(p.sample_rate, l);
    l->append(L", ");
    LogParam(p.bits_per_sample, l);
    l->append(L", ");
    LogParam(p.packet_size, l);
    l->append(L")");
  }
};


#if defined(OS_POSIX)

// TODO(port): this shouldn't exist. However, the plugin stuff is really using
// HWNDS (NativeView), and making Windows calls based on them. I've not figured
// out the deal with plugins yet.
template <>
struct ParamTraits<gfx::NativeView> {
  typedef gfx::NativeView param_type;
  static void Write(Message* m, const param_type& p) {
    NOTIMPLEMENTED();
  }

  static bool Read(const Message* m, void** iter, param_type* p) {
    NOTIMPLEMENTED();
    *p = NULL;
    return true;
  }

  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"<gfx::NativeView>"));
  }
};

#endif  // defined(OS_POSIX)

template <>
struct ParamTraits<AudioOutputStream::State> {
  typedef AudioOutputStream::State param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<AudioOutputStream::State>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring state;
    switch (p) {
     case AudioOutputStream::STATE_CREATED:
       state = L"AudioOutputStream::STATE_CREATED";
       break;
     case AudioOutputStream::STATE_STARTED:
       state = L"AudioOutputStream::STATE_STARTED";
       break;
     case AudioOutputStream::STATE_PAUSED:
       state = L"AudioOutputStream::STATE_PAUSED";
       break;
     case AudioOutputStream::STATE_STOPPED:
       state = L"AudioOutputStream::STATE_STOPPED";
       break;
     case AudioOutputStream::STATE_CLOSED:
       state = L"AudioOutputStream::STATE_CLOSED";
       break;
     case AudioOutputStream::STATE_ERROR:
       state = L"AudioOutputStream::STATE_ERROR";
       break;
     default:
      state = L"UNKNOWN";
      break;
    }

    LogParam(state, l);
  }
};

template <>
struct ParamTraits<WebAppCacheContext::ContextType> {
  typedef WebAppCacheContext::ContextType param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(static_cast<int>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<param_type>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring state;
    switch (p) {
     case WebAppCacheContext::MAIN_FRAME:
      state = L"MAIN_FRAME";
      break;
     case WebAppCacheContext::CHILD_FRAME:
      state = L"CHILD_FRAME";
      break;
     case WebAppCacheContext::DEDICATED_WORKER:
       state = L"DECICATED_WORKER";
       break;
     default:
      state = L"UNKNOWN";
      break;
    }

    LogParam(state, l);
  }
};

template<>
struct ParamTraits<WebMenuItem::Type> {
  typedef WebMenuItem::Type param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<WebMenuItem::Type>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring type;
    switch (p) {
      case WebMenuItem::OPTION:
        type = L"OPTION";
        break;
      case WebMenuItem::GROUP:
        type = L"GROUP";
        break;
      case WebMenuItem::SEPARATOR:
        type = L"SEPARATOR";
        break;
      default:
        type = L"UNKNOWN";
        break;
    }
    LogParam(type, l);
  }
};

template<>
struct ParamTraits<WebMenuItem> {
  typedef WebMenuItem param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.label);
    WriteParam(m, p.type);
    WriteParam(m, p.enabled);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
        ReadParam(m, iter, &p->label) &&
        ReadParam(m, iter, &p->type) &&
        ReadParam(m, iter, &p->enabled);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.label, l);
    l->append(L", ");
    LogParam(p.type, l);
    l->append(L", ");
    LogParam(p.enabled, l);
    l->append(L")");
  }
};

// Traits for ViewHostMsg_ShowPopup_Params.
template <>
struct ParamTraits<ViewHostMsg_ShowPopup_Params> {
  typedef ViewHostMsg_ShowPopup_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.bounds);
    WriteParam(m, p.item_height);
    WriteParam(m, p.selected_item);
    WriteParam(m, p.popup_items);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
        ReadParam(m, iter, &p->bounds) &&
        ReadParam(m, iter, &p->item_height) &&
        ReadParam(m, iter, &p->selected_item) &&
        ReadParam(m, iter, &p->popup_items);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.bounds, l);
    l->append(L", ");
    LogParam(p.item_height, l);
    l->append(L", ");
    LogParam(p.selected_item, l);
    l->append(L", ");
    LogParam(p.popup_items, l);
    l->append(L")");
  }
};

}  // namespace IPC


#define MESSAGES_INTERNAL_FILE "chrome/common/render_messages_internal.h"
#include "chrome/common/ipc_message_macros.h"

#endif  // CHROME_COMMON_RENDER_MESSAGES_H_
