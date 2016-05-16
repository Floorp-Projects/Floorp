/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NOTIFICATION_TYPE_H_
#define CHROME_COMMON_NOTIFICATION_TYPE_H_

// This file describes various types used to describe and filter notifications
// that pass through the NotificationService.
//
// It is written as an enum inside a class so that it can be forward declared.
// You're not allowed to forward declare an enum, and we want to forward
// declare this since it's required by NotificationObserver which is included
// by a lot of header files.
//
// Since this class encapsulates an integral value, it should be passed by
// value.
class NotificationType {
 public:
  enum Type {
    // General -----------------------------------------------------------------

    // Special signal value to represent an interest in all notifications.
    // Not valid when posting a notification.
    ALL = 0,

    // The app is done processing user actions, now is a good time to do
    // some background work.
    IDLE,

    // Means that the app has just started doing something in response to a
    // user action, and that background processes shouldn't run if avoidable.
    BUSY,

    // This is sent when the user does a gesture resulting in a noteworthy
    // action taking place. This is typically used for logging. The source is
    // the profile, and the details is a wstring identifying the action.
    USER_ACTION,

    // NavigationController ----------------------------------------------------

    // A new pending navigation has been created. Pending entries are created
    // when the user requests the navigation. We don't know if it will actually
    // happen until it does (at this point, it will be "committed." Note that
    // renderer- initiated navigations such as link clicks will never be
    // pending.
    //
    // This notification is called after the pending entry is created, but
    // before we actually try to navigate. The source will be the
    // NavigationController that owns the pending entry, and there are no
    // details.
    NAV_ENTRY_PENDING,

    // A new non-pending navigation entry has been created. This will
    // correspond to one NavigationController entry being created (in the case
    // of new navigations) or renavigated to (for back/forward navigations).
    //
    // The source will be the navigation controller doing the commit. The
    // details will be NavigationController::LoadCommittedDetails.
    NAV_ENTRY_COMMITTED,

    // Indicates that the NavigationController given in the Source has
    // decreased its back/forward list count by removing entries from either
    // the front or back of its list. This is usually the result of going back
    // and then doing a new navigation, meaning all the "forward" items are
    // deleted.
    //
    // This normally happens as a result of a new navigation. It will be
    // followed by a NAV_ENTRY_COMMITTED message for the new page that
    // caused the pruning. It could also be a result of removing an item from
    // the list to fix up after interstitials.
    //
    // The details are NavigationController::PrunedDetails.
    NAV_LIST_PRUNED,

    // Indicates that a NavigationEntry has changed. The source will be the
    // NavigationController that owns the NavigationEntry. The details will be
    // a NavigationController::EntryChangedDetails struct.
    //
    // This will NOT be sent on navigation, interested parties should also
    // listen for NAV_ENTRY_COMMITTED to handle that case. This will be
    // sent when the entry is updated outside of navigation (like when a new
    // title comes).
    NAV_ENTRY_CHANGED,

    // Other load-related (not from NavigationController) ----------------------

    // A content load is starting.  The source will be a
    // Source<NavigationController> corresponding to the tab in which the load
    // is occurring.  No details are expected for this notification.
    LOAD_START,

    // A content load has stopped. The source will be a
    // Source<NavigationController> corresponding to the tab in which the load
    // is occurring.  Details in the form of a LoadNotificationDetails object
    // are optional.
    LOAD_STOP,

    // A frame is staring a provisional load.  The source is a
    // Source<NavigationController> corresponding to the tab in which the load
    // occurs.  Details is a bool specifying if the load occurs in the main
    // frame (or a sub-frame if false).
    FRAME_PROVISIONAL_LOAD_START,

    // Content was loaded from an in-memory cache.  The source will be a
    // Source<NavigationController> corresponding to the tab in which the load
    // occurred.  Details in the form of a LoadFromMemoryCacheDetails object
    // are provided.
    LOAD_FROM_MEMORY_CACHE,

    // A provisional content load has failed with an error.  The source will be
    // a Source<NavigationController> corresponding to the tab in which the
    // load occurred.  Details in the form of a ProvisionalLoadDetails object
    // are provided.
    FAIL_PROVISIONAL_LOAD_WITH_ERROR,

    // A response has been received for a resource request.  The source will be
    // a Source<NavigationController> corresponding to the tab in which the
    // request was issued.  Details in the form of a ResourceRequestDetails
    // object are provided.
    RESOURCE_RESPONSE_STARTED,

    // The response to a resource request has completed.  The source will be a
    // Source<NavigationController> corresponding to the tab in which the
    // request was issued.  Details in the form of a ResourceRequestDetails
    // object are provided.
    RESOURCE_RESPONSE_COMPLETED,

    // A redirect was received while requesting a resource.  The source will be
    // a Source<NavigationController> corresponding to the tab in which the
    // request was issued.  Details in the form of a ResourceRedirectDetails
    // are provided.
    RESOURCE_RECEIVED_REDIRECT,

    // The SSL state of a page has changed in some visible way.  For example,
    // if an insecure resource is loaded on a secure page.  Note that a
    // toplevel load commit will also update the SSL state (since the
    // NavigationEntry is new) and this message won't always be sent in that
    // case.  Listen to this notification if you need to refresh SSL-related UI
    // elements.
    //
    // The source will be the navigation controller associated with the load.
    // There are no details.  The entry changed will be the active entry of the
    // controller.
    SSL_VISIBLE_STATE_CHANGED,

    // The SSL state of the browser has changed in some internal way.  For
    // example, the user might have explicitly allowed some broken certificate
    // or a secure origin might have included some insecure content.  Listen to
    // this notifiation if you need to keep track of our internal SSL state.
    //
    // The source will be the navigation controller associated with the state
    // change.  There are no details.
    SSL_INTERNAL_STATE_CHANGED,

    // Lets resource handlers and other interested observers know when the
    // message filter is being deleted and can no longer be used.
    RESOURCE_MESSAGE_FILTER_SHUTDOWN,

    // Views -------------------------------------------------------------------

    // Notification that a view was removed from a view hierarchy.  The source
    // is the view, the details is the parent view.
    VIEW_REMOVED,

    // Browser-window ----------------------------------------------------------

    // This message is sent after a window has been opened.  The source is a
    // Source<Browser> with a pointer to the new window.  No details are
    // expected.
    BROWSER_OPENED,

    // This message is sent after a window has been closed.  The source is a
    // Source<Browser> with a pointer to the closed window.  Details is a
    // boolean that if true indicates that the application will be closed as a
    // result of this browser window closure (i.e. this was the last opened
    // browser window).  Note that the boolean pointed to by Details is only
    // valid for the duration of this call.
    BROWSER_CLOSED,

    // This message is sent when the last window considered to be an
    // "application window" has been closed. Dependent/dialog/utility windows
    // can use this as a way to know that they should also close. No source or
    // details are passed.
    ALL_APPWINDOWS_CLOSED,

    // Indicates a new top window has been created. The source is the
    // WindowWin.
    WINDOW_CREATED,

    // Indicates that a top window has been closed.  The source is the HWND
    // that was closed, no details are expected.
    WINDOW_CLOSED,

    // Sent when an info bubble has been created but not yet shown. The source
    // is the InfoBubble.
    INFO_BUBBLE_CREATED,

    // Tabs --------------------------------------------------------------------

    // This notification is sent after a tab has been appended to the
    // tab_strip.  The source is a Source<NavigationController> with a pointer
    // to controller for the added tab. There are no details.
    TAB_PARENTED,

    // This message is sent before a tab has been closed.  The source is a
    // Source<NavigationController> with a pointer to the controller for the
    // closed tab.  No details are expected.
    //
    // See also TAB_CLOSED.
    TAB_CLOSING,

    // Notification that a tab has been closed. The source is the
    // NavigationController with no details.
    TAB_CLOSED,

    // This notification is sent when a render view host has connected to a
    // renderer process. The source is a Source<TabContents> with a pointer to
    // the TabContents.  A TAB_CONTENTS_DISCONNECTED notification is
    // guaranteed before the source pointer becomes junk.  No details are
    // expected.
    TAB_CONTENTS_CONNECTED,

    // This notification is sent when a TabContents swaps its render view host
    // with another one, possibly changing processes. The source is a
    // Source<TabContents> with a pointer to the TabContents.  A
    // TAB_CONTENTS_DISCONNECTED notification is guaranteed before the
    // source pointer becomes junk.  No details are expected.
    TAB_CONTENTS_SWAPPED,

    // This message is sent after a TabContents is disconnected from the
    // renderer process.  The source is a Source<TabContents> with a pointer to
    // the TabContents (the pointer is usable).  No details are expected.
    TAB_CONTENTS_DISCONNECTED,

    // This message is sent when a new InfoBar has been added to a TabContents.
    // The source is a Source<TabContents> with a pointer to the TabContents
    // the InfoBar was added to. The details is a Details<InfoBarDelegate> with
    // a pointer to an object implementing the InfoBarDelegate interface for
    // the InfoBar that was added.
    TAB_CONTENTS_INFOBAR_ADDED,

    // This message is sent when an InfoBar is about to be removed from a
    // TabContents. The source is a Source<TabContents> with a pointer to the
    // TabContents the InfoBar was removed from. The details is a
    // Details<InfoBarDelegate> with a pointer to an object implementing the
    // InfoBarDelegate interface for the InfoBar that was removed.
    TAB_CONTENTS_INFOBAR_REMOVED,

    // This is sent when an externally hosted tab is created. The details
    // contain the ExternalTabContainer that contains the tab
    EXTERNAL_TAB_CREATED,

    // This is sent when an externally hosted tab is closed.  No details are
    // expected.
    EXTERNAL_TAB_CLOSED,

    // Indicates that the new page tab has finished loading. This is used for
    // performance testing to see how fast we can load it after startup, and is
    // only called once for the lifetime of the browser. The source is unused.
    // Details is an integer: the number of milliseconds elapsed between
    // starting and finishing all painting.
    INITIAL_NEW_TAB_UI_LOAD,

    // This notification is sent when a TabContents is being hidden, e.g. due
    // to switching away from this tab.  The source is a Source<TabContents>.
    TAB_CONTENTS_HIDDEN,

    // This notification is sent when a TabContents is being destroyed. Any
    // object holding a reference to a TabContents can listen to that
    // notification to properly reset the reference. The source is a
    // Source<TabContents>.
    TAB_CONTENTS_DESTROYED,

    // Stuff inside the tabs ---------------------------------------------------

    // This message is sent after a constrained window has been closed.  The
    // source is a Source<ConstrainedWindow> with a pointer to the closed child
    // window.  (The pointer isn't usable, except for identification.) No
    // details are expected.
    CWINDOW_CLOSED,

    // Indicates that a RenderProcessHost is destructing. The source will be the
    // RenderProcessHost that corresponds to the process.
    RENDERER_PROCESS_TERMINATED,

    // Indicates that a render process was closed (meaning it exited, but the
    // RenderProcessHost might be reused).  The source will be the corresponding
    // RenderProcessHost.  The details will be a bool which is true if the
    // process crashed.  This may get sent along with
    // RENDERER_PROCESS_TERMINATED.
    RENDERER_PROCESS_CLOSED,

    // Indicates that a render process has become unresponsive for a period of
    // time. The source will be the RenderWidgetHost that corresponds to the
    // hung view, and no details are expected.
    RENDERER_PROCESS_HANG,

    // Indicates that a render process is created in the sandbox. The source
    // will be the RenderProcessHost that corresponds to the created process
    // and the detail is a bool telling us if the process got created on the
    // sandbox desktop or not.
    RENDERER_PROCESS_IN_SBOX,

    // This is sent to notify that the RenderViewHost displayed in a
    // TabContents has changed.  Source is the TabContents for which the change
    // happened, details is the previous RenderViewHost (can be NULL when the
    // first RenderViewHost is set).
    RENDER_VIEW_HOST_CHANGED,

    // This is sent when a RenderWidgetHost is being destroyed. The source is
    // the RenderWidgetHost, the details are not used.
    RENDER_WIDGET_HOST_DESTROYED,

    // Notification from TabContents that we have received a response from the
    // renderer after using the dom inspector.
    DOM_INSPECT_ELEMENT_RESPONSE,

    // Notification from TabContents that we have received a response from the
    // renderer in response to a dom automation controller action.
    DOM_OPERATION_RESPONSE,

    // Sent when the bookmark bubble hides. The source is the profile, the
    // details unused.
    BOOKMARK_BUBBLE_HIDDEN,

    // This notification is sent when the result of a find-in-page search is
    // available with the browser process. The source is a Source<TabContents>
    // with a pointer to the TabContents. Details encompass a
    // FindNotificationDetail object that tells whether the match was found or
    // not found.
    FIND_RESULT_AVAILABLE,

    // This is sent when the users preference for when the bookmark bar should
    // be shown changes. The source is the profile, and the details are
    // NoDetails.
    BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,

    // Used to monitor web cache usage by notifying whenever the
    // CacheManagerHost observes new UsageStats. The source will be the
    // RenderProcessHost that corresponds to the new statistics. Details are a
    // UsageStats object sent by the renderer, and should be copied - ptr not
    // guaranteed to be valid after the notification.
    WEB_CACHE_STATS_OBSERVED,

    // Child Processes ---------------------------------------------------------

    // This notification is sent when a child process host has connected to a
    // child process.  There is no usable source, since it is sent from an
    // ephemeral task; register for AllSources() to receive this notification.
    // The details are in a Details<ChildProcessInfo>.
    CHILD_PROCESS_HOST_CONNECTED,

    // This message is sent after a ChildProcessHost is disconnected from the
    // child process.  There is no usable source, since it is sent from an
    // ephemeral task; register for AllSources() to receive this notification.
    // The details are in a Details<ChildProcessInfo>.
    CHILD_PROCESS_HOST_DISCONNECTED,

    // This message is sent when a child process disappears unexpectedly.
    // There is no usable source, since it is sent from an ephemeral task;
    // register for AllSources() to receive this notification.  The details are
    // in a Details<ChildProcessInfo>.
    CHILD_PROCESS_CRASHED,

    // This message indicates that an instance of a particular child was
    // created in a page.  (If one page contains several regions rendered by
    // the same child, this notification will occur once for each region
    // during the page load.)
    //
    // There is no usable source, since it is sent from an ephemeral task;
    // register for AllSources() to receive this notification.  The details are
    // in a Details<ChildProcessInfo>.
    CHILD_INSTANCE_CREATED,

    // This is sent when network interception is disabled for a plugin, or the
    // plugin is unloaded.  This should only be sent/received on the browser IO
    // thread or the plugin thread. The source is the plugin that is disabling
    // interception.  No details are expected.
    CHROME_PLUGIN_UNLOADED,

    // This is sent when a login prompt is shown.  The source is the
    // Source<NavigationController> for the tab in which the prompt is shown.
    // Details are a LoginNotificationDetails which provide the LoginHandler
    // that should be given authentication.
    AUTH_NEEDED,

    // This is sent when authentication credentials have been supplied (either
    // by the user or by an automation service), but before we've actually
    // received another response from the server.  The source is the
    // Source<NavigationController> for the tab in which the prompt was shown.
    // No details are expected.
    AUTH_SUPPLIED,

    // History -----------------------------------------------------------------

    // Sent when a history service is created on the main thread. This is sent
    // after history is created, but before it has finished loading. Use
    // HISTORY_LOADED is you need to know when loading has completed.
    // The source is the profile that the history service belongs to, and the
    // details is the pointer to the newly created HistoryService object.
    HISTORY_CREATED,

    // Sent when a history service has finished loading. The source is the
    // profile that the history service belongs to, and the details is the
    // HistoryService.
    HISTORY_LOADED,

    // Sent when a URL that has been typed has been added or modified. This is
    // used by the in-memory URL database (used by autocomplete) to track
    // changes to the main history system.
    //
    // The source is the profile owning the history service that changed, and
    // the details is history::URLsModifiedDetails that lists the modified or
    // added URLs.
    HISTORY_TYPED_URLS_MODIFIED,

    // Sent when the user visits a URL.
    //
    // The source is the profile owning the history service that changed, and
    // the details is history::URLVisitedDetails.
    HISTORY_URL_VISITED,

    // Sent when one or more URLs are deleted.
    //
    // The source is the profile owning the history service that changed, and
    // the details is history::URLsDeletedDetails that lists the deleted URLs.
    HISTORY_URLS_DELETED,

    // Sent by history when the favicon of a URL changes.  The source is the
    // profile, and the details is history::FavIconChangeDetails (see
    // history_notifications.h).
    FAVICON_CHANGED,

    // Bookmarks ---------------------------------------------------------------

    // Sent when the starred state of a URL changes. A URL is starred if there
    // is at least one bookmark for it. The source is a Profile and the details
    // is history::URLsStarredDetails that contains the list of URLs and
    // whether they were starred or unstarred.
    URLS_STARRED,

    // Sent when the bookmark bar model finishes loading. This source is the
    // Profile, and the details aren't used.
    BOOKMARK_MODEL_LOADED,

    // Sent when the spellchecker object changes. Note that this is not sent
    // the first time the spellchecker gets initialized. The source is the
    // profile, the details is SpellcheckerReinitializedDetails defined in
    // profile.
    SPELLCHECKER_REINITIALIZED,

    // Sent when the bookmark bubble is shown for a particular URL. The source
    // is the profile, the details the URL.
    BOOKMARK_BUBBLE_SHOWN,

    // Non-history storage services --------------------------------------------

    // Notification that the TemplateURLModel has finished loading from the
    // database. The source is the TemplateURLModel, and the details are
    // NoDetails.
    TEMPLATE_URL_MODEL_LOADED,

    // Notification triggered when a web application has been installed or
    // uninstalled. Any application view should reload its data.  The source is
    // the profile. No details are provided.
    WEB_APP_INSTALL_CHANGED,

    // This is sent to a pref observer when a pref is changed.
    PREF_CHANGED,

    // Sent when a default request context has been created, so calling
    // Profile::GetDefaultRequestContext() will not return NULL.  This is sent
    // on the thread where Profile::GetRequestContext() is first called, which
    // should be the UI thread.
    DEFAULT_REQUEST_CONTEXT_AVAILABLE,

    // Autocomplete ------------------------------------------------------------

    // Sent by the autocomplete controller at least once per query, each time
    // new matches are available, subject to rate-limiting/coalescing to reduce
    // the number of updates.  There are no details.
    AUTOCOMPLETE_CONTROLLER_RESULT_UPDATED,

    // Sent by the autocomplete controller once per query, immediately after
    // synchronous matches become available.  There are no details.
    AUTOCOMPLETE_CONTROLLER_SYNCHRONOUS_MATCHES_AVAILABLE,

    // This is sent when an item of the Omnibox popup is selected. The source
    // is the profile.
    OMNIBOX_OPENED_URL,

    // Sent by the autocomplete edit when it is destroyed.
    AUTOCOMPLETE_EDIT_DESTROYED,

    // Sent when the main Google URL has been updated.  Some services cache
    // this value and need to update themselves when it changes.  See
    // google_util::GetGoogleURLAndUpdateIfNecessary().
    GOOGLE_URL_UPDATED,

    // Printing ----------------------------------------------------------------

    // Notification from a PrintedDocument that it has been updated. It may be
    // that a printed page has just been generated or that the document's
    // number of pages has been calculated. Details is the new page or NULL if
    // only the number of pages in the document has been updated.
    PRINTED_DOCUMENT_UPDATED,

    // Notification from PrintJob that an event occured. It can be that a page
    // finished printing or that the print job failed. Details is
    // PrintJob::EventDetails.
    PRINT_JOB_EVENT,

    // Shutdown ----------------------------------------------------------------

    // Sent on the browser IO thread when an URLRequestContext is released by
    // its owning Profile.  The source is a pointer to the URLRequestContext.
    URL_REQUEST_CONTEXT_RELEASED,

    // Sent when WM_ENDSESSION has been received, after the browsers have been
    // closed but before browser process has been shutdown. The source/details
    // are all source and no details.
    SESSION_END,

    // Personalization ---------------------------------------------------------

    PERSONALIZATION,
    PERSONALIZATION_CREATED,

    // User Scripts ------------------------------------------------------------

    // Sent when there are new user scripts available.  The details are a
    // pointer to SharedMemory containing the new scripts.
    USER_SCRIPTS_LOADED,

    // Extensions --------------------------------------------------------------

    // Sent when new extensions are loaded. The details are an ExtensionList*.
    EXTENSIONS_LOADED,

    // Sent when new extensions are installed. The details are a FilePath.
    EXTENSION_INSTALLED,

    // Debugging ---------------------------------------------------------------

    // Sent from ~RenderViewHost. The source is the RenderViewHost.
    RENDER_VIEW_HOST_DELETED,

    // Count (must be last) ----------------------------------------------------
    // Used to determine the number of notification types.  Not valid as
    // a type parameter when registering for or posting notifications.
    NOTIFICATION_TYPE_COUNT
  };

  explicit NotificationType(Type v) : value(v) {}

  bool operator==(NotificationType t) const { return value == t.value; }
  bool operator!=(NotificationType t) const { return value != t.value; }

  // Comparison to explicit enum values.
  bool operator==(Type v) const { return value == v; }
  bool operator!=(Type v) const { return value != v; }

  Type value;
};

inline bool operator==(NotificationType::Type a, NotificationType b) {
  return a == b.value;
}
inline bool operator!=(NotificationType::Type a, NotificationType b) {
  return a != b.value;
}

#endif  // CHROME_COMMON_NOTIFICATION_TYPE_H_
