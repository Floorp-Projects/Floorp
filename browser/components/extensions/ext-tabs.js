/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

XPCOMUtils.defineLazyModuleGetter(this, "MatchPattern",
                                  "resource://gre/modules/MatchPattern.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PromiseUtils",
                                  "resource://gre/modules/PromiseUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");

var {
  SingletonEventManager,
  ignoreEvent,
} = ExtensionUtils;

// This function is pretty tightly tied to Extension.jsm.
// Its job is to fill in the |tab| property of the sender.
function getSender(extension, target, sender) {
  let tabId;
  if ("tabId" in sender) {
    // The message came from a privileged extension page running in a tab. In
    // that case, it should include a tabId property (which is filled in by the
    // page-open listener below).
    tabId = sender.tabId;
    delete sender.tabId;
  } else if (target instanceof Ci.nsIDOMXULElement) {
    tabId = tabTracker.getBrowserData(target).tabId;
  }

  if (tabId) {
    let tab = extension.tabManager.get(tabId, null);
    if (tab) {
      sender.tab = tab.convert();
    }
  }
}

// Used by Extension.jsm
global.tabGetSender = getSender;

/* eslint-disable mozilla/balanced-listeners */
extensions.on("page-shutdown", (type, context) => {
  if (context.viewType == "tab") {
    if (context.extension.id !== context.xulBrowser.contentPrincipal.addonId) {
      // Only close extension tabs.
      // This check prevents about:addons from closing when it contains a
      // WebExtension as an embedded inline options page.
      return;
    }
    let {gBrowser} = context.xulBrowser.ownerGlobal;
    if (gBrowser) {
      let tab = gBrowser.getTabForBrowser(context.xulBrowser);
      if (tab) {
        gBrowser.removeTab(tab);
      }
    }
  }
});
/* eslint-enable mozilla/balanced-listeners */

let tabListener = {
  tabReadyInitialized: false,
  tabReadyPromises: new WeakMap(),
  initializingTabs: new WeakSet(),

  initTabReady() {
    if (!this.tabReadyInitialized) {
      windowTracker.addListener("progress", this);

      this.tabReadyInitialized = true;
    }
  },

  onLocationChange(browser, webProgress, request, locationURI, flags) {
    if (webProgress.isTopLevel) {
      let {gBrowser} = browser.ownerGlobal;
      let tab = gBrowser.getTabForBrowser(browser);

      // Now we are certain that the first page in the tab was loaded.
      this.initializingTabs.delete(tab);

      // browser.innerWindowID is now set, resolve the promises if any.
      let deferred = this.tabReadyPromises.get(tab);
      if (deferred) {
        deferred.resolve(tab);
        this.tabReadyPromises.delete(tab);
      }
    }
  },

  /**
   * Returns a promise that resolves when the tab is ready.
   * Tabs created via the `tabs.create` method are "ready" once the location
   * changes to the requested URL. Other tabs are assumed to be ready once their
   * inner window ID is known.
   *
   * @param {XULElement} tab The <tab> element.
   * @returns {Promise} Resolves with the given tab once ready.
   */
  awaitTabReady(tab) {
    let deferred = this.tabReadyPromises.get(tab);
    if (!deferred) {
      deferred = PromiseUtils.defer();
      if (!this.initializingTabs.has(tab) && (tab.linkedBrowser.innerWindowID ||
                                              tab.linkedBrowser.currentURI.spec === "about:blank")) {
        deferred.resolve(tab);
      } else {
        this.initTabReady();
        this.tabReadyPromises.set(tab, deferred);
      }
    }
    return deferred.promise;
  },
};

extensions.registerSchemaAPI("tabs", "addon_parent", context => {
  let {extension} = context;

  let {tabManager} = extension;

  function getTabOrActive(tabId) {
    if (tabId !== null) {
      return tabTracker.getTab(tabId);
    }
    return tabTracker.activeTab;
  }

  async function promiseTabWhenReady(tabId) {
    let tab;
    if (tabId !== null) {
      tab = tabManager.get(tabId);
    } else {
      tab = tabManager.getWrapper(tabTracker.activeTab);
    }

    await tabListener.awaitTabReady(tab.tab);

    return tab;
  }

  let self = {
    tabs: {
      onActivated: new WindowEventManager(context, "tabs.onActivated", "TabSelect", (fire, event) => {
        let tab = event.originalTarget;
        let tabId = tabTracker.getId(tab);
        let windowId = windowTracker.getId(tab.ownerGlobal);
        fire.async({tabId, windowId});
      }).api(),

      onCreated: new SingletonEventManager(context, "tabs.onCreated", fire => {
        let listener = (eventName, event) => {
          fire.async(tabManager.convert(event.tab));
        };

        tabTracker.on("tab-created", listener);
        return () => {
          tabTracker.off("tab-created", listener);
        };
      }).api(),

      /**
       * Since multiple tabs currently can't be highlighted, onHighlighted
       * essentially acts an alias for self.tabs.onActivated but returns
       * the tabId in an array to match the API.
       * @see  https://developer.mozilla.org/en-US/Add-ons/WebExtensions/API/Tabs/onHighlighted
      */
      onHighlighted: new WindowEventManager(context, "tabs.onHighlighted", "TabSelect", (fire, event) => {
        let tab = event.originalTarget;
        let tabIds = [tabTracker.getId(tab)];
        let windowId = windowTracker.getId(tab.ownerGlobal);
        fire.async({tabIds, windowId});
      }).api(),

      onAttached: new SingletonEventManager(context, "tabs.onAttached", fire => {
        let listener = (eventName, event) => {
          fire.async(event.tabId, {newWindowId: event.newWindowId, newPosition: event.newPosition});
        };

        tabTracker.on("tab-attached", listener);
        return () => {
          tabTracker.off("tab-attached", listener);
        };
      }).api(),

      onDetached: new SingletonEventManager(context, "tabs.onDetached", fire => {
        let listener = (eventName, event) => {
          fire.async(event.tabId, {oldWindowId: event.oldWindowId, oldPosition: event.oldPosition});
        };

        tabTracker.on("tab-detached", listener);
        return () => {
          tabTracker.off("tab-detached", listener);
        };
      }).api(),

      onRemoved: new SingletonEventManager(context, "tabs.onRemoved", fire => {
        let listener = (eventName, event) => {
          fire.async(event.tabId, {windowId: event.windowId, isWindowClosing: event.isWindowClosing});
        };

        tabTracker.on("tab-removed", listener);
        return () => {
          tabTracker.off("tab-removed", listener);
        };
      }).api(),

      onReplaced: ignoreEvent(context, "tabs.onReplaced"),

      onMoved: new SingletonEventManager(context, "tabs.onMoved", fire => {
        // There are certain circumstances where we need to ignore a move event.
        //
        // Namely, the first time the tab is moved after it's created, we need
        // to report the final position as the initial position in the tab's
        // onAttached or onCreated event. This is because most tabs are inserted
        // in a temporary location and then moved after the TabOpen event fires,
        // which generates a TabOpen event followed by a TabMove event, which
        // does not match the contract of our API.
        let ignoreNextMove = new WeakSet();

        let openListener = event => {
          ignoreNextMove.add(event.target);
          // Remove the tab from the set on the next tick, since it will already
          // have been moved by then.
          Promise.resolve().then(() => {
            ignoreNextMove.delete(event.target);
          });
        };

        let moveListener = event => {
          let tab = event.originalTarget;

          if (ignoreNextMove.has(tab)) {
            ignoreNextMove.delete(tab);
            return;
          }

          fire.async(tabTracker.getId(tab), {
            windowId: windowTracker.getId(tab.ownerGlobal),
            fromIndex: event.detail,
            toIndex: tab._tPos,
          });
        };

        windowTracker.addListener("TabMove", moveListener);
        windowTracker.addListener("TabOpen", openListener);
        return () => {
          windowTracker.removeListener("TabMove", moveListener);
          windowTracker.removeListener("TabOpen", openListener);
        };
      }).api(),

      onUpdated: new SingletonEventManager(context, "tabs.onUpdated", fire => {
        const restricted = ["url", "favIconUrl", "title"];

        function sanitize(extension, changeInfo) {
          let result = {};
          let nonempty = false;
          for (let prop in changeInfo) {
            if (extension.hasPermission("tabs") || !restricted.includes(prop)) {
              nonempty = true;
              result[prop] = changeInfo[prop];
            }
          }
          return [nonempty, result];
        }

        let fireForTab = (tab, changed) => {
          let [needed, changeInfo] = sanitize(extension, changed);
          if (needed) {
            fire.async(tab.id, changeInfo, tab.convert());
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
            if (changed.includes("label")) {
              needed.push("title");
            }
          } else if (event.type == "TabPinned") {
            needed.push("pinned");
          } else if (event.type == "TabUnpinned") {
            needed.push("pinned");
          }

          let tab = tabManager.getWrapper(event.originalTarget);
          let changeInfo = {};
          for (let prop of needed) {
            changeInfo[prop] = tab[prop];
          }

          fireForTab(tab, changeInfo);
        };

        let statusListener = ({browser, status, url}) => {
          let {gBrowser} = browser.ownerGlobal;
          let tabElem = gBrowser.getTabForBrowser(browser);
          if (tabElem) {
            let changed = {status};
            if (url) {
              changed.url = url;
            }

            fireForTab(tabManager.wrapTab(tabElem), changed);
          }
        };

        windowTracker.addListener("status", statusListener);
        windowTracker.addListener("TabAttrModified", listener);
        windowTracker.addListener("TabPinned", listener);
        windowTracker.addListener("TabUnpinned", listener);

        return () => {
          windowTracker.removeListener("status", statusListener);
          windowTracker.removeListener("TabAttrModified", listener);
          windowTracker.removeListener("TabPinned", listener);
          windowTracker.removeListener("TabUnpinned", listener);
        };
      }).api(),

      create(createProperties) {
        return new Promise((resolve, reject) => {
          let window = createProperties.windowId !== null ?
            windowTracker.getWindow(createProperties.windowId, context) :
            windowTracker.topWindow;

          if (!window.gBrowser) {
            let obs = (finishedWindow, topic, data) => {
              if (finishedWindow != window) {
                return;
              }
              Services.obs.removeObserver(obs, "browser-delayed-startup-finished");
              resolve(window);
            };
            Services.obs.addObserver(obs, "browser-delayed-startup-finished", false);
          } else {
            resolve(window);
          }
        }).then(window => {
          let url;

          if (createProperties.url !== null) {
            url = context.uri.resolve(createProperties.url);

            if (!context.checkLoadURL(url, {dontReportErrors: true})) {
              return Promise.reject({message: `Illegal URL: ${url}`});
            }
          }

          if (createProperties.cookieStoreId && !extension.hasPermission("cookies")) {
            return Promise.reject({message: `No permission for cookieStoreId: ${createProperties.cookieStoreId}`});
          }

          let options = {};
          if (createProperties.cookieStoreId) {
            if (!global.isValidCookieStoreId(createProperties.cookieStoreId)) {
              return Promise.reject({message: `Illegal cookieStoreId: ${createProperties.cookieStoreId}`});
            }

            let privateWindow = PrivateBrowsingUtils.isBrowserPrivate(window.gBrowser);
            if (privateWindow && !global.isPrivateCookieStoreId(createProperties.cookieStoreId)) {
              return Promise.reject({message: `Illegal to set non-private cookieStoreId in a private window`});
            }

            if (!privateWindow && global.isPrivateCookieStoreId(createProperties.cookieStoreId)) {
              return Promise.reject({message: `Illegal to set private cookieStoreId in a non-private window`});
            }

            if (global.isContainerCookieStoreId(createProperties.cookieStoreId)) {
              let containerId = global.getContainerForCookieStoreId(createProperties.cookieStoreId);
              if (!containerId) {
                return Promise.reject({message: `No cookie store exists with ID ${createProperties.cookieStoreId}`});
              }

              options.userContextId = containerId;
            }
          }

          // Make sure things like about:blank and data: URIs never inherit,
          // and instead always get a NullPrincipal.
          options.disallowInheritPrincipal = true;

          tabListener.initTabReady();
          let tab = window.gBrowser.addTab(url || window.BROWSER_NEW_TAB_URL, options);

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

          if (createProperties.url && createProperties.url !== window.BROWSER_NEW_TAB_URL) {
            // We can't wait for a location change event for about:newtab,
            // since it may be pre-rendered, in which case its initial
            // location change event has already fired.

            // Mark the tab as initializing, so that operations like
            // `executeScript` wait until the requested URL is loaded in
            // the tab before dispatching messages to the inner window
            // that contains the URL we're attempting to load.
            tabListener.initializingTabs.add(tab);
          }

          return tabManager.convert(tab);
        });
      },

      async remove(tabs) {
        if (!Array.isArray(tabs)) {
          tabs = [tabs];
        }

        for (let tabId of tabs) {
          let tab = tabTracker.getTab(tabId);
          tab.ownerGlobal.gBrowser.removeTab(tab);
        }
      },

      async update(tabId, updateProperties) {
        let tab = getTabOrActive(tabId);

        let tabbrowser = tab.ownerGlobal.gBrowser;

        if (updateProperties.url !== null) {
          let url = context.uri.resolve(updateProperties.url);

          if (!context.checkLoadURL(url, {dontReportErrors: true})) {
            return Promise.reject({message: `Illegal URL: ${url}`});
          }

          tab.linkedBrowser.loadURI(url);
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

        return tabManager.convert(tab);
      },

      async reload(tabId, reloadProperties) {
        let tab = getTabOrActive(tabId);

        let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
        if (reloadProperties && reloadProperties.bypassCache) {
          flags |= Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
        }
        tab.linkedBrowser.reloadWithFlags(flags);
      },

      async get(tabId) {
        let tab = tabTracker.getTab(tabId);

        return tabManager.convert(tab);
      },

      getCurrent() {
        let tab;
        if (context.tabId) {
          tab = tabManager.get(context.tabId).convert();
        }
        return Promise.resolve(tab);
      },

      async query(queryInfo) {
        if (queryInfo.url !== null) {
          if (!extension.hasPermission("tabs")) {
            return Promise.reject({message: 'The "tabs" permission is required to use the query API with the "url" parameter'});
          }

          queryInfo = Object.assign({}, queryInfo);
          queryInfo.url = new MatchPattern(queryInfo.url);
        }

        return Array.from(tabManager.query(queryInfo, context),
                          tab => tab.convert());
      },

      captureVisibleTab(windowId, options) {
        if (!extension.hasPermission("<all_urls>")) {
          return Promise.reject({message: "The <all_urls> permission is required to use the captureVisibleTab API"});
        }

        let window = windowId == null ?
          windowTracker.topWindow :
          windowTracker.getWindow(windowId, context);

        let tab = window.gBrowser.selectedTab;
        return tabListener.awaitTabReady(tab).then(() => {
          let browser = tab.linkedBrowser;
          let recipient = {
            innerWindowID: browser.innerWindowID,
          };

          if (!options) {
            options = {};
          }
          if (options.format == null) {
            options.format = "png";
          }
          if (options.quality == null) {
            options.quality = 92;
          }

          let message = {
            options,
            width: browser.clientWidth,
            height: browser.clientHeight,
          };

          return context.sendMessage(browser.messageManager, "Extension:Capture",
                                     message, {recipient});
        });
      },

      async detectLanguage(tabId) {
        let tab = getTabOrActive(tabId);

        return tabListener.awaitTabReady(tab).then(() => {
          let browser = tab.linkedBrowser;
          let recipient = {innerWindowID: browser.innerWindowID};

          return context.sendMessage(browser.messageManager, "Extension:DetectLanguage",
                                     {}, {recipient});
        });
      },

      async executeScript(tabId, details) {
        let tab = await promiseTabWhenReady(tabId);

        return tab.executeScript(context, details);
      },

      async insertCSS(tabId, details) {
        let tab = await promiseTabWhenReady(tabId);

        return tab.insertCSS(context, details);
      },

      async removeCSS(tabId, details) {
        let tab = await promiseTabWhenReady(tabId);

        return tab.removeCSS(context, details);
      },

      async move(tabIds, moveProperties) {
        let index = moveProperties.index;
        let tabsMoved = [];
        if (!Array.isArray(tabIds)) {
          tabIds = [tabIds];
        }

        let destinationWindow = null;
        if (moveProperties.windowId !== null) {
          destinationWindow = windowTracker.getWindow(moveProperties.windowId);
          // Fail on an invalid window.
          if (!destinationWindow) {
            return Promise.reject({message: `Invalid window ID: ${moveProperties.windowId}`});
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

        let tabs = tabIds.map(tabId => tabTracker.getTab(tabId));
        for (let tab of tabs) {
          // If the window is not specified, use the window from the tab.
          let window = destinationWindow || tab.ownerGlobal;
          let gBrowser = window.gBrowser;

          let insertionPoint = indexMap.get(window) || index;
          // If the index is -1 it should go to the end of the tabs.
          if (insertionPoint == -1) {
            insertionPoint = gBrowser.tabs.length;
          }

          // We can only move pinned tabs to a point within, or just after,
          // the current set of pinned tabs. Unpinned tabs, likewise, can only
          // be moved to a position after the current set of pinned tabs.
          // Attempts to move a tab to an illegal position are ignored.
          let numPinned = gBrowser._numPinnedTabs;
          let ok = tab.pinned ? insertionPoint <= numPinned : insertionPoint >= numPinned;
          if (!ok) {
            continue;
          }

          indexMap.set(window, insertionPoint + 1);

          if (tab.ownerGlobal != window) {
            // If the window we are moving the tab in is different, then move the tab
            // to the new window.
            tab = gBrowser.adoptTab(tab, insertionPoint, false);
          } else {
            // If the window we are moving is the same, just move the tab.
            gBrowser.moveTabTo(tab, insertionPoint);
          }
          tabsMoved.push(tab);
        }

        return tabsMoved.map(tab => tabManager.convert(tab));
      },

      duplicate(tabId) {
        let tab = tabTracker.getTab(tabId);

        let gBrowser = tab.ownerGlobal.gBrowser;
        let newTab = gBrowser.duplicateTab(tab);

        return new Promise(resolve => {
          // We need to use SSTabRestoring because any attributes set before
          // are ignored. SSTabRestored is too late and results in a jump in
          // the UI. See http://bit.ly/session-store-api for more information.
          newTab.addEventListener("SSTabRestoring", function() {
            // As the tab is restoring, move it to the correct position.

            // Pinned tabs that are duplicated are inserted
            // after the existing pinned tab and pinned.
            if (tab.pinned) {
              gBrowser.pinTab(newTab);
            }
            gBrowser.moveTabTo(newTab, tab._tPos + 1);
          }, {once: true});

          newTab.addEventListener("SSTabRestored", function() {
            // Once it has been restored, select it and return the promise.
            gBrowser.selectedTab = newTab;

            resolve(tabManager.convert(newTab));
          }, {once: true});
        });
      },

      getZoom(tabId) {
        let tab = getTabOrActive(tabId);

        let {ZoomManager} = tab.ownerGlobal;
        let zoom = ZoomManager.getZoomForBrowser(tab.linkedBrowser);

        return Promise.resolve(zoom);
      },

      setZoom(tabId, zoom) {
        let tab = getTabOrActive(tabId);

        let {FullZoom, ZoomManager} = tab.ownerGlobal;

        if (zoom === 0) {
          // A value of zero means use the default zoom factor.
          return FullZoom.reset(tab.linkedBrowser);
        } else if (zoom >= ZoomManager.MIN && zoom <= ZoomManager.MAX) {
          FullZoom.setZoom(zoom, tab.linkedBrowser);
        } else {
          return Promise.reject({
            message: `Zoom value ${zoom} out of range (must be between ${ZoomManager.MIN} and ${ZoomManager.MAX})`,
          });
        }

        return Promise.resolve();
      },

      _getZoomSettings(tabId) {
        let tab = getTabOrActive(tabId);

        let {FullZoom} = tab.ownerGlobal;

        return {
          mode: "automatic",
          scope: FullZoom.siteSpecific ? "per-origin" : "per-tab",
          defaultZoomFactor: 1,
        };
      },

      getZoomSettings(tabId) {
        return Promise.resolve(this._getZoomSettings(tabId));
      },

      setZoomSettings(tabId, settings) {
        let tab = getTabOrActive(tabId);

        let currentSettings = this._getZoomSettings(tab.id);

        if (!Object.keys(settings).every(key => settings[key] === currentSettings[key])) {
          return Promise.reject(`Unsupported zoom settings: ${JSON.stringify(settings)}`);
        }
        return Promise.resolve();
      },

      onZoomChange: new SingletonEventManager(context, "tabs.onZoomChange", fire => {
        let getZoomLevel = browser => {
          let {ZoomManager} = browser.ownerGlobal;

          return ZoomManager.getZoomForBrowser(browser);
        };

        // Stores the last known zoom level for each tab's browser.
        // WeakMap[<browser> -> number]
        let zoomLevels = new WeakMap();

        // Store the zoom level for all existing tabs.
        for (let window of windowTracker.browserWindows()) {
          for (let tab of window.gBrowser.tabs) {
            let browser = tab.linkedBrowser;
            zoomLevels.set(browser, getZoomLevel(browser));
          }
        }

        let tabCreated = (eventName, event) => {
          let browser = event.tab.linkedBrowser;
          zoomLevels.set(browser, getZoomLevel(browser));
        };


        let zoomListener = event => {
          let browser = event.originalTarget;

          // For non-remote browsers, this event is dispatched on the document
          // rather than on the <browser>.
          if (browser instanceof Ci.nsIDOMDocument) {
            browser = browser.defaultView.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIDocShell)
                             .chromeEventHandler;
          }

          let {gBrowser} = browser.ownerGlobal;
          let tab = gBrowser.getTabForBrowser(browser);
          if (!tab) {
            // We only care about zoom events in the top-level browser of a tab.
            return;
          }

          let oldZoomFactor = zoomLevels.get(browser);
          let newZoomFactor = getZoomLevel(browser);

          if (oldZoomFactor != newZoomFactor) {
            zoomLevels.set(browser, newZoomFactor);

            let tabId = tabTracker.getId(tab);
            fire.async({
              tabId,
              oldZoomFactor,
              newZoomFactor,
              zoomSettings: self.tabs._getZoomSettings(tabId),
            });
          }
        };

        tabTracker.on("tab-attached", tabCreated);
        tabTracker.on("tab-created", tabCreated);

        windowTracker.addListener("FullZoomChange", zoomListener);
        windowTracker.addListener("TextZoomChange", zoomListener);
        return () => {
          tabTracker.off("tab-attached", tabCreated);
          tabTracker.off("tab-created", tabCreated);

          windowTracker.removeListener("FullZoomChange", zoomListener);
          windowTracker.removeListener("TextZoomChange", zoomListener);
        };
      }).api(),
    },
  };
  return self;
});
