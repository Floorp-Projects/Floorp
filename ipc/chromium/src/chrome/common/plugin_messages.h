// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines messages between the browser and plugin process, as well as between
// the renderer and plugin process.
//
// See render_message* for information about the multi-pass include of headers.

#ifndef CHROME_COMMON_PLUGIN_MESSAGES_H__
#define CHROME_COMMON_PLUGIN_MESSAGES_H__

#include <string>
#include <vector>

#include "base/gfx/native_widget_types.h"
#include "base/gfx/rect.h"
#include "base/basictypes.h"
#include "chrome/common/ipc_message_utils.h"
#include "googleurl/src/gurl.h"
#include "third_party/npapi/bindings/npapi.h"
#include "webkit/glue/npruntime_util.h"

// Name prefix of the event handle when a message box is displayed.
#define kMessageBoxEventPrefix L"message_box_active"

// Structures for messages that have too many parameters to be put in a
// predefined IPC message.

struct PluginMsg_Init_Params {
  gfx::NativeViewId containing_window;
  GURL url;
  std::vector<std::string> arg_names;
  std::vector<std::string> arg_values;
#if defined(OS_WIN)
  HANDLE modal_dialog_event;
#endif
  bool load_manually;
};

struct PluginHostMsg_URLRequest_Params {
  std::string method;
  bool is_javascript_url;
  std::string target;
  std::vector<char> buffer;
  bool is_file_data;
  bool notify;
  std::string url;
  intptr_t notify_data;
  bool popups_allowed;
};

struct PluginMsg_URLRequestReply_Params {
  int resource_id;
  std::string url;
  bool notify_needed;
  intptr_t notify_data;
  intptr_t stream;
};

struct PluginMsg_DidReceiveResponseParams {
  int id;
  std::string mime_type;
  std::string headers;
  uint32_t expected_length;
  uint32_t last_modified;
  bool request_is_seekable;
};

struct NPIdentifier_Param {
  NPIdentifier identifier;
};

enum NPVariant_ParamEnum {
  NPVARIANT_PARAM_VOID,
  NPVARIANT_PARAM_NULL,
  NPVARIANT_PARAM_BOOL,
  NPVARIANT_PARAM_INT,
  NPVARIANT_PARAM_DOUBLE,
  NPVARIANT_PARAM_STRING,
  // Used when when the NPObject is running in the caller's process, so we
  // create an NPObjectProxy in the other process.
  NPVARIANT_PARAM_OBJECT_ROUTING_ID,
  // Used when the NPObject we're sending is running in the callee's process
  // (i.e. we have an NPObjectProxy for it).  In that case we want the callee
  // to just use the raw pointer.
  NPVARIANT_PARAM_OBJECT_POINTER,
};

struct NPVariant_Param {
  NPVariant_ParamEnum type;
  bool bool_value;
  int int_value;
  double double_value;
  std::string string_value;
  int npobject_routing_id;
  intptr_t npobject_pointer;
};


namespace IPC {

// Traits for PluginMsg_Init_Params structure to pack/unpack.
template <>
struct ParamTraits<PluginMsg_Init_Params> {
  typedef PluginMsg_Init_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.containing_window);
    WriteParam(m, p.url);
    DCHECK(p.arg_names.size() == p.arg_values.size());
    WriteParam(m, p.arg_names);
    WriteParam(m, p.arg_values);
#if defined(OS_WIN)
    WriteParam(m, p.modal_dialog_event);
#endif
    WriteParam(m, p.load_manually);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->containing_window) &&
           ReadParam(m, iter, &p->url) &&
           ReadParam(m, iter, &p->arg_names) &&
           ReadParam(m, iter, &p->arg_values) &&
#if defined(OS_WIN)
           ReadParam(m, iter, &p->modal_dialog_event) &&
#endif
           ReadParam(m, iter, &p->load_manually);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.containing_window, l);
    l->append(L", ");
    LogParam(p.url, l);
    l->append(L", ");
    LogParam(p.arg_names, l);
    l->append(L", ");
    LogParam(p.arg_values, l);
    l->append(L", ");
#if defined(OS_WIN)
    LogParam(p.modal_dialog_event, l);
    l->append(L", ");
#endif
    LogParam(p.load_manually, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<PluginHostMsg_URLRequest_Params> {
  typedef PluginHostMsg_URLRequest_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.method);
    WriteParam(m, p.is_javascript_url);
    WriteParam(m, p.target);
    WriteParam(m, p.buffer);
    WriteParam(m, p.is_file_data);
    WriteParam(m, p.notify);
    WriteParam(m, p.url);
    WriteParam(m, p.notify_data);
    WriteParam(m, p.popups_allowed);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->method) &&
      ReadParam(m, iter, &p->is_javascript_url) &&
      ReadParam(m, iter, &p->target) &&
      ReadParam(m, iter, &p->buffer) &&
      ReadParam(m, iter, &p->is_file_data) &&
      ReadParam(m, iter, &p->notify) &&
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->notify_data) &&
      ReadParam(m, iter, &p->popups_allowed);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.method, l);
    l->append(L", ");
    LogParam(p.is_javascript_url, l);
    l->append(L", ");
    LogParam(p.target, l);
    l->append(L", ");
    LogParam(p.buffer, l);
    l->append(L", ");
    LogParam(p.is_file_data, l);
    l->append(L", ");
    LogParam(p.notify, l);
    l->append(L", ");
    LogParam(p.url, l);
    l->append(L", ");
    LogParam(p.notify_data, l);
    l->append(L", ");
    LogParam(p.popups_allowed, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<PluginMsg_URLRequestReply_Params> {
  typedef PluginMsg_URLRequestReply_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.resource_id);
    WriteParam(m, p.url);
    WriteParam(m, p.notify_needed);
    WriteParam(m, p.notify_data);
    WriteParam(m, p.stream);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->resource_id) &&
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->notify_needed) &&
      ReadParam(m, iter, &p->notify_data) &&
      ReadParam(m, iter, &p->stream);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.resource_id, l);
    l->append(L", ");
    LogParam(p.url, l);
    l->append(L", ");
    LogParam(p.notify_needed, l);
    l->append(L", ");
    LogParam(p.notify_data, l);
    l->append(L", ");
    LogParam(p.stream, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<PluginMsg_DidReceiveResponseParams> {
  typedef PluginMsg_DidReceiveResponseParams param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.id);
    WriteParam(m, p.mime_type);
    WriteParam(m, p.headers);
    WriteParam(m, p.expected_length);
    WriteParam(m, p.last_modified);
    WriteParam(m, p.request_is_seekable);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->id) &&
      ReadParam(m, iter, &r->mime_type) &&
      ReadParam(m, iter, &r->headers) &&
      ReadParam(m, iter, &r->expected_length) &&
      ReadParam(m, iter, &r->last_modified) &&
      ReadParam(m, iter, &r->request_is_seekable);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.id, l);
    l->append(L", ");
    LogParam(p.mime_type, l);
    l->append(L", ");
    LogParam(p.headers, l);
    l->append(L", ");
    LogParam(p.expected_length, l);
    l->append(L", ");
    LogParam(p.last_modified, l);
    l->append(L", ");
    LogParam(p.request_is_seekable, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<NPEvent> {
  typedef NPEvent param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteData(reinterpret_cast<const char*>(&p), sizeof(NPEvent));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    const char *data;
    int data_size = 0;
    bool result = m->ReadData(iter, &data, &data_size);
    if (!result || data_size != sizeof(NPEvent)) {
      NOTREACHED();
      return false;
    }

    memcpy(r, data, sizeof(NPEvent));
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
#if defined(OS_WIN)
    std::wstring event, wparam, lparam;
    lparam = StringPrintf(L"(%d, %d)", LOWORD(p.lParam), HIWORD(p.lParam));
    switch(p.event) {
     case WM_KEYDOWN:
      event = L"WM_KEYDOWN";
      wparam = StringPrintf(L"%d", p.wParam);
      lparam = StringPrintf(L"%d", p.lParam);
      break;
     case WM_KEYUP:
      event = L"WM_KEYDOWN";
      wparam = StringPrintf(L"%d", p.wParam);
      lparam = StringPrintf(L"%x", p.lParam);
      break;
     case WM_MOUSEMOVE:
      event = L"WM_MOUSEMOVE";
      if (p.wParam & MK_LBUTTON) {
        wparam = L"MK_LBUTTON";
      } else if (p.wParam & MK_MBUTTON) {
        wparam = L"MK_MBUTTON";
      } else if (p.wParam & MK_RBUTTON) {
        wparam = L"MK_RBUTTON";
      }
      break;
     case WM_LBUTTONDOWN:
      event = L"WM_LBUTTONDOWN";
      break;
     case WM_MBUTTONDOWN:
      event = L"WM_MBUTTONDOWN";
      break;
     case WM_RBUTTONDOWN:
      event = L"WM_RBUTTONDOWN";
      break;
     case WM_LBUTTONUP:
      event = L"WM_LBUTTONUP";
      break;
     case WM_MBUTTONUP:
      event = L"WM_MBUTTONUP";
      break;
     case WM_RBUTTONUP:
      event = L"WM_RBUTTONUP";
      break;
    }

    if (p.wParam & MK_CONTROL) {
      if (!wparam.empty())
        wparam += L" ";
      wparam += L"MK_CONTROL";
    }

    if (p.wParam & MK_SHIFT) {
      if (!wparam.empty())
        wparam += L" ";
      wparam += L"MK_SHIFT";
    }

    l->append(L"(");
    LogParam(event, l);
    l->append(L", ");
    LogParam(wparam, l);
    l->append(L", ");
    LogParam(lparam, l);
    l->append(L")");
#else
    l->append(L"<NPEvent>");
#endif
  }
};

template <>
struct ParamTraits<NPIdentifier_Param> {
  typedef NPIdentifier_Param param_type;
  static void Write(Message* m, const param_type& p) {
    webkit_glue::SerializeNPIdentifier(p.identifier, m);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return webkit_glue::DeserializeNPIdentifier(*m, iter, &r->identifier);
  }
  static void Log(const param_type& p, std::wstring* l) {
    if (NPN_IdentifierIsString(p.identifier)) {
      NPUTF8* str = NPN_UTF8FromIdentifier(p.identifier);
      l->append(UTF8ToWide(str));
      NPN_MemFree(str);
    } else {
      l->append(IntToWString(NPN_IntFromIdentifier(p.identifier)));
    }
  }
};

template <>
struct ParamTraits<NPVariant_Param> {
  typedef NPVariant_Param param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.type));
    if (p.type == NPVARIANT_PARAM_BOOL) {
      WriteParam(m, p.bool_value);
    } else if (p.type == NPVARIANT_PARAM_INT) {
      WriteParam(m, p.int_value);
    } else if (p.type == NPVARIANT_PARAM_DOUBLE) {
      WriteParam(m, p.double_value);
    } else if (p.type == NPVARIANT_PARAM_STRING) {
      WriteParam(m, p.string_value);
    } else if (p.type == NPVARIANT_PARAM_OBJECT_ROUTING_ID) {
      // This is the routing id used to connect NPObjectProxy in the other
      // process with NPObjectStub in this process.
      WriteParam(m, p.npobject_routing_id);
      // The actual NPObject pointer, in case it's passed back to this process.
      WriteParam(m, p.npobject_pointer);
    } else if (p.type == NPVARIANT_PARAM_OBJECT_POINTER) {
      // The NPObject resides in the other process, so just send its pointer.
      WriteParam(m, p.npobject_pointer);
    } else {
      DCHECK(p.type == NPVARIANT_PARAM_VOID || p.type == NPVARIANT_PARAM_NULL);
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int type;
    if (!ReadParam(m, iter, &type))
      return false;

    bool result = false;
    r->type = static_cast<NPVariant_ParamEnum>(type);
    if (r->type == NPVARIANT_PARAM_BOOL) {
      result = ReadParam(m, iter, &r->bool_value);
    } else if (r->type == NPVARIANT_PARAM_INT) {
      result = ReadParam(m, iter, &r->int_value);
    } else if (r->type == NPVARIANT_PARAM_DOUBLE) {
      result = ReadParam(m, iter, &r->double_value);
    } else if (r->type == NPVARIANT_PARAM_STRING) {
      result = ReadParam(m, iter, &r->string_value);
    } else if (r->type == NPVARIANT_PARAM_OBJECT_ROUTING_ID) {
      result =
          ReadParam(m, iter, &r->npobject_routing_id) &&
          ReadParam(m, iter, &r->npobject_pointer);
    } else if (r->type == NPVARIANT_PARAM_OBJECT_POINTER) {
      result = ReadParam(m, iter, &r->npobject_pointer);
    } else if ((r->type == NPVARIANT_PARAM_VOID) ||
               (r->type == NPVARIANT_PARAM_NULL)) {
      result = true;
    } else {
      NOTREACHED();
    }

    return result;
  }
  static void Log(const param_type& p, std::wstring* l) {
    if (p.type == NPVARIANT_PARAM_BOOL) {
      LogParam(p.bool_value, l);
    } else if (p.type == NPVARIANT_PARAM_INT) {
      LogParam(p.int_value, l);
    } else if (p.type == NPVARIANT_PARAM_DOUBLE) {
      LogParam(p.double_value, l);
    } else if (p.type == NPVARIANT_PARAM_STRING) {
      LogParam(p.string_value, l);
    } else if (p.type == NPVARIANT_PARAM_OBJECT_ROUTING_ID) {
      LogParam(p.npobject_routing_id, l);
      LogParam(p.npobject_pointer, l);
    } else if (p.type == NPVARIANT_PARAM_OBJECT_POINTER) {
      LogParam(p.npobject_pointer, l);
    }
  }
};

}  // namespace IPC


#define MESSAGES_INTERNAL_FILE "chrome/common/plugin_messages_internal.h"
#include "chrome/common/ipc_message_macros.h"

#endif  // CHROME_COMMON_PLUGIN_MESSAGES_H__
