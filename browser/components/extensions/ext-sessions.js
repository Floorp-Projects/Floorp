/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-utils.js */

var {
  ExtensionError,
  promiseObserved,
} = ExtensionUtils;

XPCOMUtils.defineLazyModuleGetter(this, "SessionStore",
                                  "resource:///modules/sessionstore/SessionStore.jsm");

const SS_ON_CLOSED_OBJECTS_CHANGED = "sessionstore-closed-objects-changed";

function getRecentlyClosed(maxResults, extension) {
  let recentlyClosed = [];

  // Get closed windows
  let closedWindowData = SessionStore.getClosedWindowData(false);
  for (let window of closedWindowData) {
    recentlyClosed.push({
      lastModified: window.closedAt,
      window: Window.convertFromSessionStoreClosedData(extension, window)});
  }

  // Get closed tabs
  for (let window of windowTracker.browserWindows()) {
    let closedTabData = SessionStore.getClosedTabData(window, false);
    for (let tab of closedTabData) {
      recentlyClosed.push({
        lastModified: tab.closedAt,
        tab: Tab.convertFromSessionStoreClosedData(extension, tab, window)});
    }
  }

  // Sort windows and tabs
  recentlyClosed.sort((a, b) => b.lastModified - a.lastModified);
  return recentlyClosed.slice(0, maxResults);
}

async function createSession(restored, extension, sessionId) {
  if (!restored) {
    throw new ExtensionError(`Could not restore object using sessionId ${sessionId}.`);
  }
  let sessionObj = {lastModified: Date.now()};
  if (restored instanceof Ci.nsIDOMChromeWindow) {
    await promiseObserved("sessionstore-single-window-restored", subject => subject == restored);
    sessionObj.window = extension.windowManager.convert(restored, {populate: true});
    return sessionObj;
  }
  sessionObj.tab = extension.tabManager.convert(restored);
  return sessionObj;
}

this.sessions = class extends ExtensionAPI {
  getAPI(context) {
    let {extension} = context;
    return {
      sessions: {
        async getRecentlyClosed(filter) {
          await SessionStore.promiseInitialized;
          let maxResults = filter.maxResults == undefined ? this.MAX_SESSION_RESULTS : filter.maxResults;
          return getRecentlyClosed(maxResults, extension);
        },

        async forgetClosedTab(windowId, sessionId) {
          await SessionStore.promiseInitialized;
          let window = context.extension.windowManager.get(windowId).window;
          let closedTabData = SessionStore.getClosedTabData(window, false);

          let closedTabIndex = closedTabData.findIndex((closedTab) => {
            return closedTab.closedId === parseInt(sessionId, 10);
          });

          if (closedTabIndex < 0) {
            throw new ExtensionError(`Could not find closed tab using sessionId ${sessionId}.`);
          }

          SessionStore.forgetClosedTab(window, closedTabIndex);
        },

        async forgetClosedWindow(sessionId) {
          await SessionStore.promiseInitialized;
          let closedWindowData = SessionStore.getClosedWindowData(false);

          let closedWindowIndex = closedWindowData.findIndex((closedWindow) => {
            return closedWindow.closedId === parseInt(sessionId, 10);
          });

          if (closedWindowIndex < 0) {
            throw new ExtensionError(`Could not find closed window using sessionId ${sessionId}.`);
          }

          SessionStore.forgetClosedWindow(closedWindowIndex);
        },

        async restore(sessionId) {
          await SessionStore.promiseInitialized;
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
            for (let window of windowTracker.browserWindows()) {
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

        onChanged: new SingletonEventManager(context, "sessions.onChanged", fire => {
          let observer = () => {
            fire.async();
          };

          Services.obs.addObserver(observer, SS_ON_CLOSED_OBJECTS_CHANGED);
          return () => {
            Services.obs.removeObserver(observer, SS_ON_CLOSED_OBJECTS_CHANGED);
          };
        }).api(),
      },
    };
  }
};
