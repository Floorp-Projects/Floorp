// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/shared_memory.h"
#include "chrome/common/ipc_message_macros.h"

// PluginProcess messages
// These are messages sent from the browser to the plugin process.
IPC_BEGIN_MESSAGES(PluginProcess)
  // Tells the plugin process to create a new channel for communication with a
  // renderer.  The channel name is returned in a
  // PluginProcessHostMsg_ChannelCreated message.
  IPC_MESSAGE_CONTROL1(PluginProcessMsg_CreateChannel,
                       bool /* off_the_record */)

  // Allows a chrome plugin loaded in the browser process to send arbitrary
  // data to an instance of the same plugin loaded in a plugin process.
  IPC_MESSAGE_CONTROL1(PluginProcessMsg_PluginMessage,
                       std::vector<uint8_t> /* opaque data */)

  // The following messages are used by all child processes, even though they
  // are listed under PluginProcess.  It seems overkill to define ChildProcess.
  // Tells the child process it should stop.
  IPC_MESSAGE_CONTROL0(PluginProcessMsg_AskBeforeShutdown)

  // Sent in response to PluginProcessHostMsg_ShutdownRequest to tell the child
  // process that it's safe to shutdown.
  IPC_MESSAGE_CONTROL0(PluginProcessMsg_Shutdown)

IPC_END_MESSAGES(PluginProcess)


//-----------------------------------------------------------------------------
// PluginProcessHost messages
// These are messages sent from the plugin process to the browser process.
IPC_BEGIN_MESSAGES(PluginProcessHost)
  // Response to a PluginProcessMsg_CreateChannel message.
  IPC_MESSAGE_CONTROL1(PluginProcessHostMsg_ChannelCreated,
                       std::wstring /* channel_name */)

  IPC_SYNC_MESSAGE_CONTROL0_1(PluginProcessHostMsg_GetPluginFinderUrl,
                              std::string /* plugin finder URL */)

  IPC_MESSAGE_CONTROL0(PluginProcessHostMsg_ShutdownRequest)

  // Allows a chrome plugin loaded in a plugin process to send arbitrary
  // data to an instance of the same plugin loaded in the browser process.
  IPC_MESSAGE_CONTROL1(PluginProcessHostMsg_PluginMessage,
                       std::vector<uint8_t> /* opaque data */)

  // Allows a chrome plugin loaded in a plugin process to send arbitrary
  // data to an instance of the same plugin loaded in the browser process.
  IPC_SYNC_MESSAGE_CONTROL1_1(PluginProcessHostMsg_PluginSyncMessage,
                              std::vector<uint8_t> /* opaque data */,
                              std::vector<uint8_t> /* opaque data response */)

  // Used to get cookies for the given URL.  The request_context is a
  // CPBrowsingContext, but is passed as int32_t to avoid compilation errors.
  IPC_SYNC_MESSAGE_CONTROL2_1(PluginProcessHostMsg_GetCookies,
                              int32_t /* request_context */,
                              GURL /* url */,
                              std::string /* cookies */)

  // Get the list of proxies to use for |url|, as a semicolon delimited list
  // of "<TYPE> <HOST>:<PORT>" | "DIRECT". See also ViewHostMsg_ResolveProxy
  // which does the same thing.
  IPC_SYNC_MESSAGE_CONTROL1_2(PluginProcessHostMsg_ResolveProxy,
                              GURL /* url */,
                              int /* network error */,
                              std::string /* proxy list */)

#if defined(OS_WIN)
  // Creates a child window of the given parent window on the UI thread.
  IPC_SYNC_MESSAGE_CONTROL1_1(PluginProcessHostMsg_CreateWindow,
                              HWND /* parent */,
                              HWND /* child */)

  // Destroys the given window's parent on the UI thread.
  IPC_MESSAGE_CONTROL2(PluginProcessHostMsg_PluginWindowDestroyed,
                       HWND /* window */,
                       HWND /* parent */)

  IPC_MESSAGE_ROUTED3(PluginProcessHostMsg_DownloadUrl,
                      std::string /* URL */,
                      int /* process id */,
                      HWND /* caller window */)
#endif

IPC_END_MESSAGES(PluginProcessHost)


//-----------------------------------------------------------------------------
// Plugin messages
// These are messages sent from the renderer process to the plugin process.
IPC_BEGIN_MESSAGES(Plugin)
  // Tells the plugin process to create a new plugin instance with the given
  // id.  A corresponding WebPluginDelegateStub is created which hosts the
  // WebPluginDelegateImpl.
  IPC_SYNC_MESSAGE_CONTROL1_1(PluginMsg_CreateInstance,
                              std::string /* mime_type */,
                              int /* instance_id */)

  // The WebPluginDelegateProxy sends this to the WebPluginDelegateStub in its
  // destructor, so that the stub deletes the actual WebPluginDelegateImpl
  // object that it's hosting.
  IPC_SYNC_MESSAGE_CONTROL1_0(PluginMsg_DestroyInstance,
                              int /* instance_id */)

  IPC_SYNC_MESSAGE_CONTROL0_1(PluginMsg_GenerateRouteID,
                             int /* id */)

  // The messages below all map to WebPluginDelegate methods.
  IPC_SYNC_MESSAGE_ROUTED1_1(PluginMsg_Init,
                             PluginMsg_Init_Params,
                             bool /* result */)

  // Used to synchronously request a paint for windowless plugins.
  IPC_SYNC_MESSAGE_ROUTED1_0(PluginMsg_Paint,
                             gfx::Rect /* damaged_rect */)

  // Sent by the renderer after it paints from its backing store so that the
  // plugin knows it can send more invalidates.
  IPC_MESSAGE_ROUTED0(PluginMsg_DidPaint)

  IPC_SYNC_MESSAGE_ROUTED0_2(PluginMsg_Print,
                             base::SharedMemoryHandle /* shared_memory*/,
                             size_t /* size */)

  IPC_SYNC_MESSAGE_ROUTED0_2(PluginMsg_GetPluginScriptableObject,
                             int /* route_id */,
                             intptr_t /* npobject_ptr */)

  IPC_SYNC_MESSAGE_ROUTED1_0(PluginMsg_DidFinishLoadWithReason,
                             int /* reason */)

  // Updates the plugin location.  For windowless plugins, windowless_buffer
  // contains a buffer that the plugin draws into.  background_buffer is used
  // for transparent windowless plugins, and holds the background of the plugin
  // rectangle.
  IPC_MESSAGE_ROUTED4(PluginMsg_UpdateGeometry,
                      gfx::Rect /* window_rect */,
                      gfx::Rect /* clip_rect */,
                      TransportDIB::Id /* windowless_buffer */,
                      TransportDIB::Id /* background_buffer */)

  IPC_SYNC_MESSAGE_ROUTED0_0(PluginMsg_SetFocus)

  IPC_SYNC_MESSAGE_ROUTED1_2(PluginMsg_HandleEvent,
                             NPEvent /* event */,
                             bool /* handled */,
                             WebCursor /* cursor type*/)

  IPC_SYNC_MESSAGE_ROUTED2_0(PluginMsg_WillSendRequest,
                             int /* id */,
                             GURL /* url */)

  IPC_SYNC_MESSAGE_ROUTED1_1(PluginMsg_DidReceiveResponse,
                             PluginMsg_DidReceiveResponseParams,
                             bool /* cancel */)

  IPC_SYNC_MESSAGE_ROUTED3_0(PluginMsg_DidReceiveData,
                             int /* id */,
                             std::vector<char> /* buffer */,
                             int /* data_offset */)

  IPC_SYNC_MESSAGE_ROUTED1_0(PluginMsg_DidFinishLoading,
                             int /* id */)

  IPC_SYNC_MESSAGE_ROUTED1_0(PluginMsg_DidFail,
                             int /* id */)

  IPC_MESSAGE_ROUTED5(PluginMsg_SendJavaScriptStream,
                      std::string /* url */,
                      std::wstring /* result */,
                      bool /* success */,
                      bool /* notify required */,
                      intptr_t /* notify data */)

  IPC_MESSAGE_ROUTED2(PluginMsg_DidReceiveManualResponse,
                      std::string /* url */,
                      PluginMsg_DidReceiveResponseParams)

  IPC_MESSAGE_ROUTED1(PluginMsg_DidReceiveManualData,
                      std::vector<char> /* buffer */)

  IPC_MESSAGE_ROUTED0(PluginMsg_DidFinishManualLoading)

  IPC_MESSAGE_ROUTED0(PluginMsg_DidManualLoadFail)

  IPC_MESSAGE_ROUTED0(PluginMsg_InstallMissingPlugin)

  IPC_SYNC_MESSAGE_ROUTED1_0(PluginMsg_HandleURLRequestReply,
                             PluginMsg_URLRequestReply_Params)

  IPC_SYNC_MESSAGE_ROUTED3_0(PluginMsg_URLRequestRouted,
                             std::string /* url */,
                             bool /* notify_needed */,
                             intptr_t /* notify data */)
IPC_END_MESSAGES(Plugin)


//-----------------------------------------------------------------------------
// PluginHost messages
// These are messages sent from the plugin process to the renderer process.
// They all map to the corresponding WebPlugin methods.
IPC_BEGIN_MESSAGES(PluginHost)
  // Sends the plugin window information to the renderer.
  // The window parameter is a handle to the window if the plugin is a windowed
  // plugin. It is NULL for windowless plugins.
  IPC_SYNC_MESSAGE_ROUTED1_0(PluginHostMsg_SetWindow,
                             gfx::NativeViewId /* window */)

#if defined(OS_WIN)
  // The modal_loop_pump_messages_event parameter is an event handle which is
  // passed in for windowless plugins and is used to indicate if messages
  // are to be pumped in sync calls to the plugin process. Currently used
  // in HandleEvent calls.
  IPC_SYNC_MESSAGE_ROUTED1_0(PluginHostMsg_SetWindowlessPumpEvent,
                             HANDLE /* modal_loop_pump_messages_event */)
#endif

  IPC_MESSAGE_ROUTED1(PluginHostMsg_URLRequest,
                      PluginHostMsg_URLRequest_Params)

  IPC_SYNC_MESSAGE_ROUTED1_0(PluginHostMsg_CancelResource,
                             int /* id */)

  IPC_MESSAGE_ROUTED1(PluginHostMsg_InvalidateRect,
                      gfx::Rect /* rect */)

  IPC_SYNC_MESSAGE_ROUTED1_2(PluginHostMsg_GetWindowScriptNPObject,
                             int /* route id */,
                             bool /* success */,
                             intptr_t /* npobject_ptr */)

  IPC_SYNC_MESSAGE_ROUTED1_2(PluginHostMsg_GetPluginElement,
                             int /* route id */,
                             bool /* success */,
                             intptr_t /* npobject_ptr */)

  IPC_MESSAGE_ROUTED3(PluginHostMsg_SetCookie,
                      GURL /* url */,
                      GURL /* policy_url */,
                      std::string /* cookie */)

  IPC_SYNC_MESSAGE_ROUTED2_1(PluginHostMsg_GetCookies,
                             GURL /* url */,
                             GURL /* policy_url */,
                             std::string /* cookies */)

  // Asks the browser to show a modal HTML dialog.  The dialog is passed the
  // given arguments as a JSON string, and returns its result as a JSON string
  // through json_retval.
  IPC_SYNC_MESSAGE_ROUTED4_1(PluginHostMsg_ShowModalHTMLDialog,
                              GURL /* url */,
                              int /* width */,
                              int /* height */,
                              std::string /* json_arguments */,
                              std::string /* json_retval */)

  IPC_MESSAGE_ROUTED1(PluginHostMsg_MissingPluginStatus,
                      int /* status */)

  IPC_SYNC_MESSAGE_ROUTED0_1(PluginHostMsg_GetCPBrowsingContext,
                             uint32_t /* context */)

  IPC_MESSAGE_ROUTED0(PluginHostMsg_CancelDocumentLoad)

  IPC_MESSAGE_ROUTED5(PluginHostMsg_InitiateHTTPRangeRequest,
                      std::string /* url */,
                      std::string /* range_info */,
                      intptr_t    /* existing_stream */,
                      bool        /* notify_needed */,
                      intptr_t    /* notify_data */)

IPC_END_MESSAGES(PluginHost)

//-----------------------------------------------------------------------------
// NPObject messages
// These are messages used to marshall NPObjects.  They are sent both from the
// plugin to the renderer and from the renderer to the plugin.
IPC_BEGIN_MESSAGES(NPObject)
  IPC_SYNC_MESSAGE_ROUTED0_0(NPObjectMsg_Release)

  IPC_SYNC_MESSAGE_ROUTED1_1(NPObjectMsg_HasMethod,
                             NPIdentifier_Param /* name */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED3_2(NPObjectMsg_Invoke,
                             bool /* is_default */,
                             NPIdentifier_Param /* method */,
                             std::vector<NPVariant_Param> /* args */,
                             NPVariant_Param /* result_param */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED1_1(NPObjectMsg_HasProperty,
                             NPIdentifier_Param /* name */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED1_2(NPObjectMsg_GetProperty,
                             NPIdentifier_Param /* name */,
                             NPVariant_Param /* property */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED2_1(NPObjectMsg_SetProperty,
                             NPIdentifier_Param /* name */,
                             NPVariant_Param /* property */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED1_1(NPObjectMsg_RemoveProperty,
                             NPIdentifier_Param /* name */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED0_0(NPObjectMsg_Invalidate)

  IPC_SYNC_MESSAGE_ROUTED0_2(NPObjectMsg_Enumeration,
                             std::vector<NPIdentifier_Param> /* value */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED2_2(NPObjectMsg_Evaluate,
                             std::string /* script */,
                             bool /* popups_allowed */,
                             NPVariant_Param /* result_param */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED1_0(NPObjectMsg_SetException,
                             std::string /* message */)

IPC_END_MESSAGES(NPObject)
