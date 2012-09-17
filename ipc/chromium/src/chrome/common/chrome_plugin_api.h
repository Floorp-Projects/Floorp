// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This header specifies the Chrome Plugin API.  It is based heavily on NPAPI.
// The key difference is that Chrome plugins can be loaded for the lifetime
// of the browser, and are not tied to a specific web page.
//
// NOTE: This API is not final and may change or go away at any point.
//
// All strings in the API are UTF8-encoded unless otherwise noted.

#ifndef CHROME_COMMON_CHROME_PLUGIN_API_H__
#define CHROME_COMMON_CHROME_PLUGIN_API_H__

#include "base/basictypes.h"

#ifndef STDCALL
#ifdef WIN32
#define STDCALL __stdcall
#else
#define STDCALL
#endif // WIN32
#endif // STDCALL

#ifdef __cplusplus
extern "C" {
#endif

// The current version of the API, used by the 'version' field of CPPluginFuncs
// and CPBrowserFuncs.
#define CP_MAJOR_VERSION 0
#define CP_MINOR_VERSION 9
#define CP_VERSION       ((CP_MAJOR_VERSION << 8) | (CP_MINOR_VERSION))

#define CP_GET_MAJOR_VERSION(version) ((version & 0xff00) >> 8)
#define CP_GET_MINOR_VERSION(version) (version & 0x00ff)

typedef unsigned char CPBool;

// Chrome plugins can be loaded into different process types.
typedef enum {
  CP_PROCESS_BROWSER = 0,
  CP_PROCESS_PLUGIN,
  CP_PROCESS_RENDERER,
} CPProcessType;

// Return codes.  Error values are negative.
typedef enum {
  // No error
  CPERR_SUCCESS = 0,

  // (network) An asynchronous IO operation is not complete
  CPERR_IO_PENDING = -1,

  // Generic failure
  CPERR_FAILURE = -2,

  // The API versions used by plugin and host are incompatible
  CPERR_INVALID_VERSION = -3,

  // The operation was cancelled
  CPERR_CANCELLED = -4,

  // An invalid parameter was passed
  CPERR_INVALID_PARAMETER = -5,
} CPError;

// Types of response info metadata to query using CPP_GetResponseInfo.
typedef enum {
  // HTTP status code.
  CPRESPONSEINFO_HTTP_STATUS = 0,

  // Raw headers from the server, including the status line.  Headers should
  // be delimited by "\0", and end with "\0\0" (a blank line).
  CPRESPONSEINFO_HTTP_RAW_HEADERS = 1,
} CPResponseInfoType;

// An identifier for the plugin used by the browser.
typedef struct _CPID_t {
  int unused;
} CPID_t;
typedef struct _CPID_t* CPID;

// An identifier that encapsulates the browsing context needed by various APIs.
// This includes information about what tab a request was made from, and what
// profile is active.  Note that this ID is global to all processes, so it can
// be passed from one process to another.  The value 0 is reserved for an
// undefined context.
typedef uint32_t CPBrowsingContext;

// Types of context info to query using CPB_GetBrowsingContextInfo.
typedef enum {
  // The data directory for the profile associated with this context as a
  // pointer to a null-terminated string.  The plugin can save persistent data
  // to this directory.  The returned pointer should be freed using CPB_Free.
  CPBROWSINGCONTEXT_DATA_DIR_PTR = 0,

  // The locale language code used for the browser UI.  The returned pointer
  // should be freed using CPB_Free.
  CPBROWSINGCONTEXT_UI_LOCALE_PTR = 1,
} CPBrowsingContextInfoType;

// A network request object.
typedef struct _CPRequest {
  void* pdata;  // plugin private data
  const char* url;  // the URL being requested
  const char* method;  // the request method as an uppercase string (ex: "GET")
  CPBrowsingContext context;  // context in which this request was made
} CPRequest;

typedef enum {
  CPREQUESTLOAD_NORMAL = 0,

  // This is "normal reload", meaning an if-none-match/if-modified-since query
  CPREQUESTLOAD_VALIDATE_CACHE = 1 << 0,

  // This is "shift-reload", meaning a "pragma: no-cache" end-to-end fetch
  CPREQUESTLOAD_BYPASS_CACHE = 1 << 1,

  // This is a back/forward style navigation where the cached content should
  // be preferred over any protocol specific cache validation.
  CPREQUESTLOAD_PREFERRING_CACHE = 1 << 2,

  // This is a navigation that will fail if it cannot serve the requested
  // resource from the cache (or some equivalent local store).
  CPREQUESTLOAD_ONLY_FROM_CACHE = 1 << 3,

  // This is a navigation that will not use the cache at all.  It does not
  // impact the HTTP request headers.
  CPREQUESTLOAD_DISABLE_CACHE = 1 << 4,

  // This navigation should not be intercepted by plugins.
  CPREQUESTLOAD_DISABLE_INTERCEPT = 1 << 5,

  // This request should be loaded synchronously.  What this means is that
  // CPR_StartRequest and CPR_Read will never return CPERR_IO_PENDING - they
  // will block until a response is available, and return success or failure.
  CPREQUESTLOAD_SYNCHRONOUS = 1 << 20,
} CPRequestLoadFlags;

//
// Functions provided by plugin to host.
//

// Called when the browser is unloading the plugin.
typedef CPError (STDCALL *CPP_ShutdownFunc)(void);

// Returns true if the plugin is interested in handling this request.
typedef CPBool (STDCALL *CPP_ShouldInterceptRequestFunc)(CPRequest* request);

// Called when an HTML dialog was closed.  json_retval is the JSON string
// containing the return value sent back by the dialog (using Chrome's
// JavaScript DOM bindings).
typedef void (STDCALL *CPP_HtmlDialogClosedFunc)(
    void* plugin_context, const char* json_retval);

// Asks the plugin to handle the given command.  'command_data' is command-
// specific data used for some builtin commands: see gears_api.h for
// possible types.  It is only valid for the duration of this call.
typedef CPError (STDCALL *CPP_HandleCommandFunc)(
    CPBrowsingContext context, int command, void* command_data);

//
// Functions provided by host to plugin.
//

// Asks the host to handle the given command.  'command_data' is
// command-specific data used for some builtin commands: see gears_api.h for
// possible types.  It is only valid for the duration of this call.
typedef CPError (STDCALL *CPB_HandleCommandFunc)(
    CPID id, CPBrowsingContext context, int command, void* command_data);

// Asks the browser to enable/disable request interception for this plugin for
// the given schemes.  'schemes' is an array of strings containing the scheme
// names the plugin wishes to handle; case is ignored.  If 'schemes' is NULL or
// empty, request interception is disabled for this plugin.  Multiple calls to
// this function will add to the existing set of enabled schemes. The browser
// should call the plugin's CPP_ShouldInterceptRequestFunc for any network
// requests it makes that match a given scheme.  The browser may choose not to
// allow the plugin to intercept certain protocols.
typedef void (STDCALL *CPB_EnableRequestInterceptFunc)(
    CPID id, const char** schemes, uint32_t num_schemes);

// Asks the browser to create a request object for the given method/url.
// Returns CPERR_SUCCESS and puts the new object into the 'request' field on
// success, or an error code on failure.  The plugin must call CPR_EndRequest
// to clean up the request object when it is done with it.
typedef CPError (STDCALL *CPB_CreateRequestFunc)(
    CPID id, CPBrowsingContext context, const char* method, const char* url,
    CPRequest** request);

// Queries the browser's cookie store for cookies set for the given URL.
// Sets 'cookies' to an allocated string containing the cookies as
// semicolon-delimited "name=value" pairs on success, NULL on failure.
// The memory should be freed using CPB_Free when done.
typedef CPError (STDCALL *CPB_GetCookiesFunc)(
    CPID id, CPBrowsingContext context, const char* url, char** cookies);

// Allocates memory for the given size using the browser's allocator.  Call
// CPB_Free when done.
typedef void* (STDCALL *CPB_AllocFunc)(uint32_t size);

// Frees a pointer allocated by CPB_Alloc.
typedef void (STDCALL *CPB_FreeFunc)(void* memory);


// Sets a flag that influences when the plugin process created to host
// the plugin is shutdown. Generally, the plugin process is terminated
// when no more plugin instances exist, this is the default behavior.
// If keep_alive is non-zero, the process will not be terminated when
// the instance count goes to zero. Note: a non-zero keep_alive value
// does not prevent the plugin process from being terminated upon
// overall browser shutdown.
typedef void (STDCALL *CPB_SetKeepProcessAliveFunc)(CPID id,
                                                    CPBool keep_alive);

// Asks the browser to show an HTML dialog to the user.  The dialog contents
// should be loaded from the given URL.  The 'json_arguments' is a JSON string
// that the dialog can fetch using Chrome's JavaScript DOM bindings.  This call
// will block until the dialog is closed.  On success, 'json_retval' will
// contain the JSON string sent back by the dialog (using Chrome's JavaScript
// DOM bindings), and CPERR_SUCCESS is returned.  'json_retval' should be freed
// using CPB_Free when done.
typedef CPError (STDCALL *CPB_ShowHtmlDialogModalFunc)(
    CPID id, CPBrowsingContext context, const char* url, int width, int height,
    const char* json_arguments, char** json_retval);

// Similar to CPB_ShowHtmlDialogModalFunc, but does not block.  When the dialog
// is closed, CPP_HtmlDialogClosed is called with the JSON return value and the
// given 'plugin_context', which may be used by the plugin to associate other
// data with the dialog.
typedef CPError (STDCALL *CPB_ShowHtmlDialogFunc)(
    CPID id, CPBrowsingContext context, const char* url, int width, int height,
    const char* json_arguments, void* plugin_context);

// Get the browsing context associated with the given NPAPI instance.
typedef CPBrowsingContext (STDCALL *CPB_GetBrowsingContextFromNPPFunc)(
    struct _NPP* npp);

// Queries for some meta data associated with the given browsing context.  See
// CPBrowsingContextInfoType for possible queries.  If buf_size is too small to
// contain the entire data, the return value will indicate the size required.
// Otherwise, the return value is a CPError or CPERR_SUCCESS.
typedef int (STDCALL *CPB_GetBrowsingContextInfoFunc)(
    CPID id, CPBrowsingContext context, CPBrowsingContextInfoType type,
    void* buf, uint32_t buf_size);

// Given an URL string, returns the string of command-line arguments that should
// be passed to start the browser at the given URL.  'arguments' should be freed
// using CPB_Free when done.
typedef CPError (STDCALL *CPB_GetCommandLineArgumentsFunc)(
    CPID id, CPBrowsingContext context, const char* url, char** arguments);

// Asks the browser to let the plugin handle the given UI command.  When the
// command is invoked, the browser will call CPP_HandleCommand.  'command'
// should be an integer identifier.  Currently only builtin commands are
// supported, but in the future we may want to let plugins add custom menu
// commands with their own descriptions.
typedef CPError (STDCALL *CPB_AddUICommandFunc)(CPID id, int command);

//
// Functions related to making network requests.
// Both the host and plugin will implement their own versions of these.
//

// Starts the request.  Returns CPERR_SUCCESS if the request could be started
// immediately, at which point the response info is available to be read.
// Returns CPERR_IO_PENDING if an asynchronous operation was started, and the
// caller should wait for CPRR_StartCompleted to be called before reading the
// response info.  Returns an error code on failure.
typedef CPError (STDCALL *CPR_StartRequestFunc)(CPRequest* request);

// Stops or cancels the request.  The caller should not access the request
// object after this call.  If an asynchronous IO operation is pending, the
// operation is aborted and the caller will not receive a callback for it.
typedef void (STDCALL *CPR_EndRequestFunc)(CPRequest* request, CPError reason);

// Sets the additional request headers to append to the standard headers that
// would normally be made with this request.  Headers should be \r\n-delimited,
// with no terminating \r\n.  Extra headers are not checked against the standard
// headers for duplicates. Must be called before CPRR_StartCompletedFunc.
// Plugins should avoid setting the following headers: User-Agent,
// Content-Length.
typedef void (STDCALL *CPR_SetExtraRequestHeadersFunc)(CPRequest* request,
                                                       const char* headers);

// Sets the load flags for this request. 'flags' is a bitwise-OR of
// CPRequestLoadFlags.  Must be called before CPRR_StartCompletedFunc.
typedef void (STDCALL *CPR_SetRequestLoadFlagsFunc)(CPRequest* request,
                                                    uint32_t flags);

// Appends binary data to the request body of a POST or PUT request.  The caller
// should set the "Content-Type" header to the appropriate mime type using
// CPR_SetExtraRequestHeadersFunc.  This can be called multiple times to append
// a sequence of data segments to upload.  Must be called before
// CPR_StartRequestFunc.
typedef void (STDCALL *CPR_AppendDataToUploadFunc)(
    CPRequest* request, const char* bytes, int bytes_len);

// Appends the contents of a file to the request body of a POST or PUT request.
// 'offset' and 'length' can be used to append a subset of the file. Pass zero
// for 'length' and 'offset' to upload the entire file. 'offset'
// indicates where the data to upload begins in the file. 'length' indicates
// how much of the file to upload. A 'length' of zero is interpretted as to
// end-of-file. If 'length' and 'offset' indicate a range beyond end of file,
// the amount sent is clipped at eof.
// See CPR_AppendDataToUploadFunc for additional usage information.
// (added in v0.4)
typedef CPError (STDCALL *CPR_AppendFileToUploadFunc)(
    CPRequest* request, const char* filepath, uint64_t offset, uint64_t length);

// Queries for some response meta data.  See CPResponseInfoType for possible
// queries.  If buf_size is too small to contain the entire data, the return
// value will indicate the size required.  Otherwise, the return value is a
// CPError or CPERR_SUCCESS.
typedef int (STDCALL *CPR_GetResponseInfoFunc)(
    CPRequest* request, CPResponseInfoType type,
    void* buf, uint32_t buf_size);

// Attempts to read a request's response data.  The number of bytes read is
// returned; 0 indicates an EOF.  CPERR_IO_PENDING is returned if an
// asynchronous operation was started, and CPRR_ReadCompletedFunc will be called
// when it completes; 'buf' must be available until the operation completes.
// Returns an error code on failure.
typedef int (STDCALL *CPR_ReadFunc)(
    CPRequest* request, void* buf, uint32_t buf_size);

//
// Functions related to serving network requests.
// Both the host and plugin will implement their own versions of these.
//

// Called upon a server-initiated redirect.  The request will still hold the
// original URL, and 'new_url' will be the redirect destination.
typedef void (STDCALL *CPRR_ReceivedRedirectFunc)(CPRequest* request,
                                                  const char* new_url);

// Called when an asynchronous CPR_StartRequest call has completed, once all
// redirects are followed.  On success, 'result' holds CPERR_SUCCESS and the
// response info is available to be read via CPR_GetResponseInfo.  On error,
// 'result' holds the error code.
typedef void (STDCALL *CPRR_StartCompletedFunc)(CPRequest* request,
                                                CPError result);

// Called when an asynchronous CPR_Read call has completed.  On success,
// 'bytes_read' will hold the number of bytes read into the buffer that was
// passed to CPR_Read; 0 indicates an EOF, and the request object will be
// destroyed after the call completes.  On failure, 'bytes_read' holds the error
// code.
typedef void (STDCALL *CPRR_ReadCompletedFunc)(CPRequest* request,
                                               int bytes_read);

// Called as upload progress is being made for async POST requests.
// (added in v0.5)
typedef void (STDCALL *CPRR_UploadProgressFunc)(CPRequest* request,
                                                uint64_t position,
                                                uint64_t size);

//
// Functions to support the sending and receipt of messages between processes.
//

// Returns true if the plugin process is running
typedef CPBool (STDCALL *CPB_IsPluginProcessRunningFunc)(CPID id);

// Returns the type of the current process.
typedef CPProcessType (STDCALL *CPB_GetProcessTypeFunc)(CPID id);

// Asks the browser to send raw data to the other process hosting an instance of
// this plugin. If needed, the plugin process will be started prior to sending
// the message.
typedef CPError (STDCALL *CPB_SendMessageFunc)(CPID id,
                                               const void *data,
                                               uint32_t data_len);

// Asks the browser to send raw data to the other process hosting an instance of
// this plugin. This function only works from the plugin or renderer process.
// This function blocks until the message is processed.  The memory should be
// freed using CPB_Free when done.
typedef CPError (STDCALL *CPB_SendSyncMessageFunc)(CPID id,
                                                   const void *data,
                                                   uint32_t data_len,
                                                   void **retval,
                                                   uint32_t *retval_len);

// This function asynchronously calls the provided function on the plugin
// thread.  user_data is passed as the argument to the function.
typedef CPError (STDCALL *CPB_PluginThreadAsyncCallFunc)(CPID id,
                                                         void (*func)(void *),
                                                         void *user_data);

// This function creates an open file dialog.  The process is granted access
// to any files that are selected.  |multiple_files| determines if more than
// one file can be selected.
typedef CPError (STDCALL *CPB_OpenFileDialogFunc)(CPID id,
                                                  CPBrowsingContext context,
                                                  bool multiple_files,
                                                  const char *title,
                                                  const char *filter,
                                                  void *user_data);

// Informs the plugin of raw data having been sent from another process.
typedef void (STDCALL *CPP_OnMessageFunc)(void *data, uint32_t data_len);

// Informs the plugin of raw data having been sent from another process.
typedef void (STDCALL *CPP_OnSyncMessageFunc)(void *data, uint32_t data_len,
                                              void **retval,
                                              uint32_t *retval_len);

// Informs the plugin that the file dialog has completed, and contains the
// results.
typedef void (STDCALL *CPP_OnFileDialogResultFunc)(void *data,
                                                   const char **files,
                                                   uint32_t files_len);

// Function table for issuing requests using via the other side's network stack.
// For the plugin, this functions deal with issuing requests through the
// browser.  For the browser, these functions deal with allowing the plugin to
// intercept requests.
typedef struct _CPRequestFuncs {
  uint16_t size;
  CPR_SetExtraRequestHeadersFunc set_extra_request_headers;
  CPR_SetRequestLoadFlagsFunc set_request_load_flags;
  CPR_AppendDataToUploadFunc append_data_to_upload;
  CPR_StartRequestFunc start_request;
  CPR_EndRequestFunc end_request;
  CPR_GetResponseInfoFunc get_response_info;
  CPR_ReadFunc read;
  CPR_AppendFileToUploadFunc append_file_to_upload;
} CPRequestFuncs;

// Function table for handling requests issued by the other side.  For the
// plugin, these deal with serving requests that the plugin has intercepted. For
// the browser, these deal with serving requests that the plugin has issued
// through us.
typedef struct _CPResponseFuncs {
  uint16_t size;
  CPRR_ReceivedRedirectFunc received_redirect;
  CPRR_StartCompletedFunc start_completed;
  CPRR_ReadCompletedFunc read_completed;
  CPRR_UploadProgressFunc upload_progress;
} CPResponseFuncs;

// Function table of CPP functions (functions provided by plugin to host). This
// structure is filled in by the plugin in the CP_Initialize call, except for
// the 'size' field, which is set by the browser.  The version fields should be
// set to those that the plugin was compiled using.
typedef struct _CPPluginFuncs {
  uint16_t size;
  uint16_t version;
  CPRequestFuncs* request_funcs;
  CPResponseFuncs* response_funcs;
  CPP_ShutdownFunc shutdown;
  CPP_ShouldInterceptRequestFunc should_intercept_request;
  CPP_OnMessageFunc on_message;
  CPP_HtmlDialogClosedFunc html_dialog_closed;
  CPP_HandleCommandFunc handle_command;
  CPP_OnSyncMessageFunc on_sync_message;
  CPP_OnFileDialogResultFunc on_file_dialog_result;
} CPPluginFuncs;

// Function table CPB functions (functions provided by host to plugin).
// This structure is filled in by the browser and provided to the plugin.  The
// plugin will likely want to save a copy of this structure to make calls
// back to the browser.
typedef struct _CPBrowserFuncs {
  uint16_t size;
  uint16_t version;
  CPRequestFuncs* request_funcs;
  CPResponseFuncs* response_funcs;
  CPB_EnableRequestInterceptFunc enable_request_intercept;
  CPB_CreateRequestFunc create_request;
  CPB_GetCookiesFunc get_cookies;
  CPB_AllocFunc alloc;
  CPB_FreeFunc free;
  CPB_SetKeepProcessAliveFunc set_keep_process_alive;
  CPB_ShowHtmlDialogModalFunc show_html_dialog_modal;
  CPB_ShowHtmlDialogFunc show_html_dialog;
  CPB_IsPluginProcessRunningFunc is_plugin_process_running;
  CPB_GetProcessTypeFunc get_process_type;
  CPB_SendMessageFunc send_message;
  CPB_GetBrowsingContextFromNPPFunc get_browsing_context_from_npp;
  CPB_GetBrowsingContextInfoFunc get_browsing_context_info;
  CPB_GetCommandLineArgumentsFunc get_command_line_arguments;
  CPB_AddUICommandFunc add_ui_command;
  CPB_HandleCommandFunc handle_command;
  CPB_SendSyncMessageFunc send_sync_message;
  CPB_PluginThreadAsyncCallFunc plugin_thread_async_call;
  CPB_OpenFileDialogFunc open_file_dialog;
} CPBrowserFuncs;


//
// DLL exports
//

// This export is optional.
// Prior to calling CP_Initialize, the browser may negotiate with the plugin
// regarding which version of the CPAPI to utilize. 'min_version' is the
// lowest version of the interface supported by the browser, 'max_version' is
// the highest supported version. The plugin can specify which version within
// the range should be used. This version will be reflected in the version field
// of the CPBrowserFuncs struct passed to CP_Initialize. If this function
// returns an error code, CP_Initialize will not be called. If function is not
// exported by the chrome plugin module, CP_Initiailize will be called with
// a version of the host's choosing.
typedef CPError (STDCALL *CP_VersionNegotiateFunc)(
  uint16_t min_version, uint16_t max_version, uint16_t *selected_version);

// 'bfuncs' are the browser functions provided to the plugin. 'id' is the
// plugin identifier that the plugin should use when calling browser functions.
// The plugin should initialize 'pfuncs' with pointers to its own functions,
// or return an error code.
// All functions and entry points should be called on the same thread.  The
// plugin should not attempt to call a browser function from a thread other
// than the one CP_InitializeFunc is called from.
typedef CPError (STDCALL *CP_InitializeFunc)(
    CPID id, const CPBrowserFuncs* bfuncs, CPPluginFuncs* pfuncs);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CHROME_COMMON_CHROME_PLUGIN_API_H__
