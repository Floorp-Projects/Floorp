/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

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

extensions.registerSchemaAPI("sessions", "addon_parent", context => {
  let {extension} = context;
  return {
    sessions: {
      getRecentlyClosed: function(filter) {
        let maxResults = filter.maxResults == undefined ? this.MAX_SESSION_RESULTS : filter.maxResults;
        return Promise.resolve(getRecentlyClosed(maxResults, extension));
      },
    },
  };
});
