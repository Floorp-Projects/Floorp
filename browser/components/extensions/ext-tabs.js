/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

XPCOMUtils.defineLazyModuleGetter(this, "MatchPattern",
                                  "resource://gre/modules/MatchPattern.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");

var {
  EventManager,
  ignoreEvent,
  runSafe,
} = ExtensionUtils;

// This function is pretty tightly tied to Extension.jsm.
// Its job is to fill in the |tab| property of the sender.
function getSender(context, target, sender) {
  // The message was sent from a content script to a <browser> element.
  // We can just get the |tab| from |target|.
  if (target instanceof Ci.nsIDOMXULElement) {
    // The message came from a content script.
    let tabbrowser = target.ownerDocument.defaultView.gBrowser;
    if (!tabbrowser) {
      return;
    }
    let tab = tabbrowser.getTabForBrowser(target);

    sender.tab = TabManager.convert(context.extension, tab);
  } else if ("tabId" in sender) {
    // The message came from an ExtensionPage. In that case, it should
    // include a tabId property (which is filled in by the page-open
    // listener below).
    sender.tab = TabManager.convert(context.extension, TabManager.getTab(sender.tabId));
    delete sender.tabId;
  }
}

// WeakMap[ExtensionPage -> {tab, parentWindow}]
var pageDataMap = new WeakMap();

/* eslint-disable mozilla/balanced-listeners */
// This listener fires whenever an extension page opens in a tab
// (either initiated by the extension or the user). Its job is to fill
// in some tab-specific details and keep data around about the
// ExtensionPage.
extensions.on("page-load", (type, page, params, sender, delegate) => {
  if (params.type == "tab" || params.type == "popup") {
    let browser = params.docShell.chromeEventHandler;

    let parentWindow = browser.ownerDocument.defaultView;
    page.windowId = WindowManager.getId(parentWindow);

    let tab = null;
    if (params.type == "tab") {
      tab = parentWindow.gBrowser.getTabForBrowser(browser);
      sender.tabId = TabManager.getId(tab);
      page.tabId = TabManager.getId(tab);
    }

    pageDataMap.set(page, {tab, parentWindow});
  }

  delegate.getSender = getSender;
});

extensions.on("page-unload", (type, page) => {
  pageDataMap.delete(page);
});

extensions.on("page-shutdown", (type, page) => {
  if (pageDataMap.has(page)) {
    let {tab, parentWindow} = pageDataMap.get(page);
    pageDataMap.delete(page);

    if (tab) {
      parentWindow.gBrowser.removeTab(tab);
    }
  }
});

extensions.on("fill-browser-data", (type, browser, data, result) => {
  let tabId = TabManager.getBrowserId(browser);
  if (tabId == -1) {
    result.cancel = true;
    return;
  }

  data.tabId = tabId;
});
/* eslint-enable mozilla/balanced-listeners */

global.currentWindow = function(context) {
  let pageData = pageDataMap.get(context);
  if (pageData) {
    return pageData.parentWindow;
  }
  return WindowManager.topWindow;
};

// TODO: activeTab permission

extensions.registerSchemaAPI("tabs", null, (extension, context) => {
  let self = {
    tabs: {
      onActivated: new WindowEventManager(context, "tabs.onActivated", "TabSelect", (fire, event) => {
        let tab = event.originalTarget;
        let tabId = TabManager.getId(tab);
        let windowId = WindowManager.getId(tab.ownerDocument.defaultView);
        fire({tabId, windowId});
      }).api(),

      onCreated: new EventManager(context, "tabs.onCreated", fire => {
        let listener = event => {
          let tab = event.originalTarget;
          fire(TabManager.convert(extension, tab));
        };

        let windowListener = window => {
          for (let tab of window.gBrowser.tabs) {
            fire(TabManager.convert(extension, tab));
          }
        };

        WindowListManager.addOpenListener(windowListener);
        AllWindowEvents.addListener("TabOpen", listener);
        return () => {
          WindowListManager.removeOpenListener(windowListener);
          AllWindowEvents.removeListener("TabOpen", listener);
        };
      }).api(),

      onUpdated: new EventManager(context, "tabs.onUpdated", fire => {
        function sanitize(extension, changeInfo) {
          let result = {};
          let nonempty = false;
          for (let prop in changeInfo) {
            if ((prop != "favIconUrl" && prop != "url") || extension.hasPermission("tabs")) {
              nonempty = true;
              result[prop] = changeInfo[prop];
            }
          }
          return [nonempty, result];
        }

        let fireForBrowser = (browser, changed) => {
          let [needed, changeInfo] = sanitize(extension, changed);
          if (needed) {
            let gBrowser = browser.ownerDocument.defaultView.gBrowser;
            let tabElem = gBrowser.getTabForBrowser(browser);

            let tab = TabManager.convert(extension, tabElem);
            fire(tab.id, changeInfo, tab);
          }
        };

        let listener = event => {
          let needed = [];
          if (event.type == "TabAttrModified") {
            let changed = event.detail.changed;
            if (changed.includes("image")) {
              needed.push("favIconUrl");
            }
            if (changed.includes("muted")) {
              needed.push("mutedInfo");
            }
            if (changed.includes("soundplaying")) {
              needed.push("audible");
            }
          } else if (event.type == "TabPinned") {
            needed.push("pinned");
          } else if (event.type == "TabUnpinned") {
            needed.push("pinned");
          }

          if (needed.length && !extension.hasPermission("tabs")) {
            needed = needed.filter(attr => attr != "url" && attr != "favIconUrl");
          }

          if (needed.length) {
            let tab = TabManager.convert(extension, event.originalTarget);

            let changeInfo = {};
            for (let prop of needed) {
              changeInfo[prop] = tab[prop];
            }
            fire(tab.id, changeInfo, tab);
          }
        };
        let progressListener = {
          onStateChange(browser, webProgress, request, stateFlags, statusCode) {
            if (!webProgress.isTopLevel) {
              return;
            }

            let status;
            if (stateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW) {
              if (stateFlags & Ci.nsIWebProgressListener.STATE_START) {
                status = "loading";
              } else if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
                status = "complete";
              }
            } else if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
                       statusCode == Cr.NS_BINDING_ABORTED) {
              status = "complete";
            }

            fireForBrowser(browser, {status});
          },

          onLocationChange(browser, webProgress, request, locationURI, flags) {
            if (!webProgress.isTopLevel) {
              return;
            }

            fireForBrowser(browser, {
              status: webProgress.isLoadingDocument ? "loading" : "complete",
              url: locationURI.spec,
            });
          },
        };

        AllWindowEvents.addListener("progress", progressListener);
        AllWindowEvents.addListener("TabAttrModified", listener);
        AllWindowEvents.addListener("TabPinned", listener);
        AllWindowEvents.addListener("TabUnpinned", listener);

        return () => {
          AllWindowEvents.removeListener("progress", progressListener);
          AllWindowEvents.removeListener("TabAttrModified", listener);
          AllWindowEvents.removeListener("TabPinned", listener);
          AllWindowEvents.removeListener("TabUnpinned", listener);
        };
      }).api(),

      onReplaced: ignoreEvent(context, "tabs.onReplaced"),

      onRemoved: new EventManager(context, "tabs.onRemoved", fire => {
        let tabListener = event => {
          let tab = event.originalTarget;
          let tabId = TabManager.getId(tab);
          let windowId = WindowManager.getId(tab.ownerDocument.defaultView);
          let removeInfo = {windowId, isWindowClosing: false};
          fire(tabId, removeInfo);
        };

        let windowListener = window => {
          for (let tab of window.gBrowser.tabs) {
            let tabId = TabManager.getId(tab);
            let windowId = WindowManager.getId(window);
            let removeInfo = {windowId, isWindowClosing: true};
            fire(tabId, removeInfo);
          }
        };

        WindowListManager.addCloseListener(windowListener);
        AllWindowEvents.addListener("TabClose", tabListener);
        return () => {
          WindowListManager.removeCloseListener(windowListener);
          AllWindowEvents.removeListener("TabClose", tabListener);
        };
      }).api(),

      create: function(createProperties, callback) {
        let url = createProperties.url || aboutNewTabService.newTabURL;
        url = context.uri.resolve(url);

        function createInWindow(window) {
          let tab = window.gBrowser.addTab(url);

          let active = true;
          if (createProperties.active !== null) {
            active = createProperties.active;
          }
          if (active) {
            window.gBrowser.selectedTab = tab;
          }

          if (createProperties.index !== null) {
            window.gBrowser.moveTabTo(tab, createProperties.index);
          }

          if (createProperties.pinned) {
            window.gBrowser.pinTab(tab);
          }

          if (callback) {
            runSafe(context, callback, TabManager.convert(extension, tab));
          }
        }

        let window = createProperties.windowId !== null ?
          WindowManager.getWindow(createProperties.windowId) :
          WindowManager.topWindow;
        if (!window.gBrowser) {
          let obs = (finishedWindow, topic, data) => {
            if (finishedWindow != window) {
              return;
            }
            Services.obs.removeObserver(obs, "browser-delayed-startup-finished");
            createInWindow(window);
          };
          Services.obs.addObserver(obs, "browser-delayed-startup-finished", false);
        } else {
          createInWindow(window);
        }
      },

      remove: function(tabs, callback) {
        if (!Array.isArray(tabs)) {
          tabs = [tabs];
        }

        for (let tabId of tabs) {
          let tab = TabManager.getTab(tabId);
          tab.ownerDocument.defaultView.gBrowser.removeTab(tab);
        }

        if (callback) {
          runSafe(context, callback);
        }
      },

      update: function(tabId, updateProperties, callback) {
        let tab = tabId !== null ? TabManager.getTab(tabId) : TabManager.activeTab;
        let tabbrowser = tab.ownerDocument.defaultView.gBrowser;
        if (updateProperties.url !== null) {
          tab.linkedBrowser.loadURI(updateProperties.url);
        }
        if (updateProperties.active !== null) {
          if (updateProperties.active) {
            tabbrowser.selectedTab = tab;
          } else {
            // Not sure what to do here? Which tab should we select?
          }
        }
        if (updateProperties.muted !== null) {
          if (tab.muted != updateProperties.muted) {
            tab.toggleMuteAudio(extension.uuid);
          }
        }
        if (updateProperties.pinned !== null) {
          if (updateProperties.pinned) {
            tabbrowser.pinTab(tab);
          } else {
            tabbrowser.unpinTab(tab);
          }
        }
        // FIXME: highlighted/selected, openerTabId

        if (callback) {
          runSafe(context, callback, TabManager.convert(extension, tab));
        }
      },

      reload: function(tabId, reloadProperties, callback) {
        let tab = tabId !== null ? TabManager.getTab(tabId) : TabManager.activeTab;
        let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
        if (reloadProperties && reloadProperties.bypassCache) {
          flags |= Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
        }
        tab.linkedBrowser.reloadWithFlags(flags);

        if (callback) {
          runSafe(context, callback);
        }
      },

      get: function(tabId, callback) {
        let tab = TabManager.getTab(tabId);
        runSafe(context, callback, TabManager.convert(extension, tab));
      },

      getCurrent(callback) {
        let tab;
        if (context.tabId) {
          tab = TabManager.convert(extension, TabManager.getTab(context.tabId));
        }
        runSafe(context, callback, tab);
      },

      getAllInWindow: function(windowId, callback) {
        if (windowId === null) {
          windowId = WindowManager.topWindow.windowId;
        }

        return self.tabs.query({windowId}, callback);
      },

      query: function(queryInfo, callback) {
        let pattern = null;
        if (queryInfo.url !== null) {
          pattern = new MatchPattern(queryInfo.url);
        }

        function matches(window, tab) {
          let props = ["active", "pinned", "highlighted", "status", "title", "index"];
          for (let prop of props) {
            if (queryInfo[prop] !== null && queryInfo[prop] != tab[prop]) {
              return false;
            }
          }

          let lastFocused = window == WindowManager.topWindow;
          if (queryInfo.lastFocusedWindow !== null && queryInfo.lastFocusedWindow != lastFocused) {
            return false;
          }

          let windowType = WindowManager.windowType(window);
          if (queryInfo.windowType !== null && queryInfo.windowType != windowType) {
            return false;
          }

          if (queryInfo.windowId !== null) {
            if (queryInfo.windowId == WindowManager.WINDOW_ID_CURRENT) {
              if (currentWindow(context) != window) {
                return false;
              }
            } else if (queryInfo.windowId != tab.windowId) {
              return false;
            }
          }

          if (queryInfo.audible !== null) {
            if (queryInfo.audible != tab.audible) {
              return false;
            }
          }

          if (queryInfo.muted !== null) {
            if (queryInfo.muted != tab.mutedInfo.muted) {
              return false;
            }
          }

          if (queryInfo.currentWindow !== null) {
            let eq = window == currentWindow(context);
            if (queryInfo.currentWindow != eq) {
              return false;
            }
          }

          if (pattern && !pattern.matches(Services.io.newURI(tab.url, null, null))) {
            return false;
          }

          return true;
        }

        let result = [];
        for (let window of WindowListManager.browserWindows()) {
          let tabs = TabManager.for(extension).getTabs(window);
          for (let tab of tabs) {
            if (matches(window, tab)) {
              result.push(tab);
            }
          }
        }
        runSafe(context, callback, result);
      },

      _execute: function(tabId, details, kind) {
        let tab = tabId !== null ? TabManager.getTab(tabId) : TabManager.activeTab;
        let mm = tab.linkedBrowser.messageManager;

        let options = {
          js: [],
          css: [],
        };

        let recipient = {
          innerWindowID: tab.linkedBrowser.innerWindowID,
        };

        if (TabManager.for(extension).hasActiveTabPermission(tab)) {
          // If we have the "activeTab" permission for this tab, ignore
          // the host whitelist.
          options.matchesHost = ["<all_urls>"];
        } else {
          options.matchesHost = extension.whiteListedHosts.serialize();
        }

        if (details.code !== null) {
          options[kind + "Code"] = details.code;
        }
        if (details.file !== null) {
          let url = context.uri.resolve(details.file);
          if (!extension.isExtensionURL(url)) {
            return Promise.reject({ message: "Files to be injected must be within the extension" });
          }
          options[kind].push(url);
        }
        if (details.allFrames) {
          options.all_frames = details.allFrames;
        }
        if (details.matchAboutBlank) {
          options.match_about_blank = details.matchAboutBlank;
        }
        if (details.runAt !== null) {
          options.run_at = details.runAt;
        }

        return context.sendMessage(mm, "Extension:Execute", { options }, recipient);
      },

      executeScript: function(tabId, details) {
        return self.tabs._execute(tabId, details, "js");
      },

      insertCSS: function(tabId, details) {
        return self.tabs._execute(tabId, details, "css");
      },

      connect: function(tabId, connectInfo) {
        let tab = TabManager.getTab(tabId);
        let mm = tab.linkedBrowser.messageManager;

        let name = "";
        if (connectInfo && connectInfo.name !== null) {
          name = connectInfo.name;
        }
        let recipient = {extensionId: extension.id};
        if (connectInfo && connectInfo.frameId !== null) {
          recipient.frameId = connectInfo.frameId;
        }
        return context.messenger.connect(mm, name, recipient);
      },

      sendMessage: function(tabId, message, options, responseCallback) {
        let tab = TabManager.getTab(tabId);
        if (!tab) {
          // ignore sendMessage to non existent tab id
          return;
        }
        let mm = tab.linkedBrowser.messageManager;

        let recipient = {extensionId: extension.id};
        if (options && options.frameId !== null) {
          recipient.frameId = options.frameId;
        }
        return context.messenger.sendMessage(mm, message, recipient, responseCallback);
      },

      move: function(tabIds, moveProperties, callback) {
        let index = moveProperties.index;
        let tabsMoved = [];
        if (!Array.isArray(tabIds)) {
          tabIds = [tabIds];
        }

        let destinationWindow = null;
        if (moveProperties.windowId !== null) {
          destinationWindow = WindowManager.getWindow(moveProperties.windowId);
          // Ignore invalid window.
          if (!destinationWindow) {
            return;
          }
        }

        /*
          Indexes are maintained on a per window basis so that a call to
            move([tabA, tabB], {index: 0})
              -> tabA to 0, tabB to 1 if tabA and tabB are in the same window
            move([tabA, tabB], {index: 0})
              -> tabA to 0, tabB to 0 if tabA and tabB are in different windows
        */
        let indexMap = new Map();

        for (let tabId of tabIds) {
          let tab = TabManager.getTab(tabId);
          // Ignore invalid tab ids.
          if (!tab) {
            continue;
          }

          // If the window is not specified, use the window from the tab.
          let window = destinationWindow || tab.ownerDocument.defaultView;
          let windowId = WindowManager.getId(window);
          let gBrowser = window.gBrowser;

          let getInsertionPoint = () => {
            let point = indexMap.get(window) || index;
            // If the index is -1 it should go to the end of the tabs.
            if (point == -1) {
              point = gBrowser.tabs.length;
            }
            indexMap.set(window, point + 1);
            return point;
          };

          if (WindowManager.getId(tab.ownerDocument.defaultView) !== windowId) {
            // If the window we are moving the tab in is different, then move the tab
            // to the new window.
            let newTab = gBrowser.addTab("about:blank");
            let newBrowser = gBrowser.getBrowserForTab(newTab);
            gBrowser.updateBrowserRemotenessByURL(newBrowser, tab.linkedBrowser.currentURI.spec);
            newBrowser.stop();
            // This is necessary for getter side-effects.
            void newBrowser.docShell;

            if (tab.pinned) {
              gBrowser.pinTab(newTab);
            }

            gBrowser.moveTabTo(newTab, getInsertionPoint());

            tab.parentNode._finishAnimateTabMove();
            gBrowser.swapBrowsersAndCloseOther(newTab, tab);
          } else {
            // If the window we are moving is the same, just move the tab.
            gBrowser.moveTabTo(tab, getInsertionPoint());
          }
          tabsMoved.push(tab);
        }

        if (callback) {
          runSafe(context, callback, tabsMoved.map(tab => TabManager.convert(extension, tab)));
        }
      },
    },
  };
  return self;
});
