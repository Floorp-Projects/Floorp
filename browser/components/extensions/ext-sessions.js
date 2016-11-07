/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  promiseObserved,
} = ExtensionUtils;

XPCOMUtils.defineLazyModuleGetter(this, "SessionStore",
                                  "resource:///modules/sessionstore/SessionStore.jsm");

function getRecentlyClosed(maxResults, extension) {
  let recentlyClosed = [];

  // Get closed windows
  let closedWindowData = SessionStore.getClosedWindowData(false);
  for (let window of closedWindowData) {
    recentlyClosed.push({
      lastModified: window.closedAt,
      window: WindowManager.convertFromSessionStoreClosedData(window, extension)});
  }

  // Get closed tabs
  for (let window of WindowListManager.browserWindows()) {
    let closedTabData = SessionStore.getClosedTabData(window, false);
    for (let tab of closedTabData) {
      recentlyClosed.push({
        lastModified: tab.closedAt,
        tab: TabManager.for(extension).convertFromSessionStoreClosedData(tab, window)});
    }
  }

  // Sort windows and tabs
  recentlyClosed.sort((a, b) => b.lastModified - a.lastModified);
  return recentlyClosed.slice(0, maxResults);
}

function createSession(restored, extension, sessionId) {
  if (!restored) {
    return Promise.reject({message: `Could not restore object using sessionId ${sessionId}.`});
  }
  let sessionObj = {lastModified: Date.now()};
  if (restored instanceof Ci.nsIDOMChromeWindow) {
    return promiseObserved("sessionstore-single-window-restored", subject => subject == restored).then(() => {
      sessionObj.window = WindowManager.convert(extension, restored, {populate: true});
      return Promise.resolve([sessionObj]);
    });
  }
  sessionObj.tab = TabManager.for(extension).convert(restored);
  return Promise.resolve([sessionObj]);
}

extensions.registerSchemaAPI("sessions", "addon_parent", context => {
  let {extension} = context;
  return {
    sessions: {
      getRecentlyClosed: function(filter) {
        let maxResults = filter.maxResults == undefined ? this.MAX_SESSION_RESULTS : filter.maxResults;
        return Promise.resolve(getRecentlyClosed(maxResults, extension));
      },
      restore: function(sessionId) {
        let session, closedId;
        if (sessionId) {
          closedId = sessionId;
          session = SessionStore.undoCloseById(closedId);
        } else if (SessionStore.lastClosedObjectType == "window") {
          // If the most recently closed object is a window, just undo closing the most recent window.
          session = SessionStore.undoCloseWindow(0);
        } else {
          // It is a tab, and we cannot call SessionStore.undoCloseTab without a window,
          // so we must find the tab in which case we can just use its closedId.
          let recentlyClosedTabs = [];
          for (let window of WindowListManager.browserWindows()) {
            let closedTabData = SessionStore.getClosedTabData(window, false);
            for (let tab of closedTabData) {
              recentlyClosedTabs.push(tab);
            }
          }

          // Sort the tabs.
          recentlyClosedTabs.sort((a, b) => b.closedAt - a.closedAt);

          // Use the closedId of the most recently closed tab to restore it.
          closedId = recentlyClosedTabs[0].closedId;
          session = SessionStore.undoCloseById(closedId);
        }
        return createSession(session, extension, closedId);
      },
    },
  };
});
