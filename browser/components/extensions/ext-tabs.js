/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-browser.js */

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PromiseUtils",
                                  "resource://gre/modules/PromiseUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "strBundle", function() {
  return Services.strings.createBundle("chrome://global/locale/extensions.properties");
});

var {
  ExtensionError,
} = ExtensionUtils;

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
      let nativeTab = gBrowser.getTabForBrowser(browser);

      // Now we are certain that the first page in the tab was loaded.
      this.initializingTabs.delete(nativeTab);

      // browser.innerWindowID is now set, resolve the promises if any.
      let deferred = this.tabReadyPromises.get(nativeTab);
      if (deferred) {
        deferred.resolve(nativeTab);
        this.tabReadyPromises.delete(nativeTab);
      }
    }
  },

  /**
   * Returns a promise that resolves when the tab is ready.
   * Tabs created via the `tabs.create` method are "ready" once the location
   * changes to the requested URL. Other tabs are assumed to be ready once their
   * inner window ID is known.
   *
   * @param {XULElement} nativeTab The <tab> element.
   * @returns {Promise} Resolves with the given tab once ready.
   */
  awaitTabReady(nativeTab) {
    let deferred = this.tabReadyPromises.get(nativeTab);
    if (!deferred) {
      deferred = PromiseUtils.defer();
      if (!this.initializingTabs.has(nativeTab) &&
          (nativeTab.linkedBrowser.innerWindowID ||
           nativeTab.linkedBrowser.currentURI.spec === "about:blank")) {
        deferred.resolve(nativeTab);
      } else {
        this.initTabReady();
        this.tabReadyPromises.set(nativeTab, deferred);
      }
    }
    return deferred.promise;
  },
};

this.tabs = class extends ExtensionAPI {
  getAPI(context) {
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

      await tabListener.awaitTabReady(tab.nativeTab);

      return tab;
    }

    let self = {
      tabs: {
        onActivated: new EventManager(context, "tabs.onActivated", fire => {
          let listener = (eventName, event) => {
            fire.async(event);
          };

          tabTracker.on("tab-activated", listener);
          return () => {
            tabTracker.off("tab-activated", listener);
          };
        }).api(),

        onCreated: new EventManager(context, "tabs.onCreated", fire => {
          let listener = (eventName, event) => {
            fire.async(tabManager.convert(event.nativeTab, event.currentTab));
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
        onHighlighted: new EventManager(context, "tabs.onHighlighted", fire => {
          let listener = (eventName, event) => {
            fire.async({tabIds: [event.tabId], windowId: event.windowId});
          };

          tabTracker.on("tab-activated", listener);
          return () => {
            tabTracker.off("tab-activated", listener);
          };
        }).api(),

        onAttached: new EventManager(context, "tabs.onAttached", fire => {
          let listener = (eventName, event) => {
            fire.async(event.tabId, {newWindowId: event.newWindowId, newPosition: event.newPosition});
          };

          tabTracker.on("tab-attached", listener);
          return () => {
            tabTracker.off("tab-attached", listener);
          };
        }).api(),

        onDetached: new EventManager(context, "tabs.onDetached", fire => {
          let listener = (eventName, event) => {
            fire.async(event.tabId, {oldWindowId: event.oldWindowId, oldPosition: event.oldPosition});
          };

          tabTracker.on("tab-detached", listener);
          return () => {
            tabTracker.off("tab-detached", listener);
          };
        }).api(),

        onRemoved: new EventManager(context, "tabs.onRemoved", fire => {
          let listener = (eventName, event) => {
            fire.async(event.tabId, {windowId: event.windowId, isWindowClosing: event.isWindowClosing});
          };

          tabTracker.on("tab-removed", listener);
          return () => {
            tabTracker.off("tab-removed", listener);
          };
        }).api(),

        onReplaced: new EventManager(context, "tabs.onReplaced", fire => {
          return () => {};
        }).api(),

        onMoved: new EventManager(context, "tabs.onMoved", fire => {
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
            let nativeTab = event.originalTarget;

            if (ignoreNextMove.has(nativeTab)) {
              ignoreNextMove.delete(nativeTab);
              return;
            }

            fire.async(tabTracker.getId(nativeTab), {
              windowId: windowTracker.getId(nativeTab.ownerGlobal),
              fromIndex: event.detail,
              toIndex: nativeTab._tPos,
            });
          };

          windowTracker.addListener("TabMove", moveListener);
          windowTracker.addListener("TabOpen", openListener);
          return () => {
            windowTracker.removeListener("TabMove", moveListener);
            windowTracker.removeListener("TabOpen", openListener);
          };
        }).api(),

        onUpdated: new EventManager(context, "tabs.onUpdated", fire => {
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
              if (changed.includes("sharing")) {
                needed.push("sharingState");
              }
            } else if (event.type == "TabPinned") {
              needed.push("pinned");
            } else if (event.type == "TabUnpinned") {
              needed.push("pinned");
            } else if (event.type == "TabBrowserInserted" &&
                       !event.detail.insertedOnTabCreation) {
              needed.push("discarded");
            } else if (event.type == "TabBrowserDiscarded") {
              needed.push("discarded");
            } else if (event.type == "TabShow") {
              needed.push("hidden");
            } else if (event.type == "TabHide") {
              needed.push("hidden");
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

          let isArticleChangeListener = (messageName, message) => {
            let {gBrowser} = message.target.ownerGlobal;
            let nativeTab = gBrowser.getTabForBrowser(message.target);

            if (nativeTab) {
              let tab = tabManager.getWrapper(nativeTab);
              fireForTab(tab, {isArticle: message.data.isArticle});
            }
          };

          windowTracker.addListener("status", statusListener);
          windowTracker.addListener("TabAttrModified", listener);
          windowTracker.addListener("TabPinned", listener);
          windowTracker.addListener("TabUnpinned", listener);
          windowTracker.addListener("TabBrowserInserted", listener);
          windowTracker.addListener("TabBrowserDiscarded", listener);
          windowTracker.addListener("TabShow", listener);
          windowTracker.addListener("TabHide", listener);

          tabTracker.on("tab-isarticle", isArticleChangeListener);

          return () => {
            windowTracker.removeListener("status", statusListener);
            windowTracker.removeListener("TabAttrModified", listener);
            windowTracker.removeListener("TabPinned", listener);
            windowTracker.removeListener("TabUnpinned", listener);
            windowTracker.removeListener("TabBrowserInserted", listener);
            windowTracker.removeListener("TabBrowserDiscarded", listener);
            windowTracker.removeListener("TabShow", listener);
            windowTracker.removeListener("TabHide", listener);
            tabTracker.off("tab-isarticle", isArticleChangeListener);
          };
        }).api(),

        create(createProperties) {
          return new Promise((resolve, reject) => {
            let window = createProperties.windowId !== null ?
              windowTracker.getWindow(createProperties.windowId, context) :
              windowTracker.topNormalWindow;

            if (!window.gBrowser) {
              let obs = (finishedWindow, topic, data) => {
                if (finishedWindow != window) {
                  return;
                }
                Services.obs.removeObserver(obs, "browser-delayed-startup-finished");
                resolve(window);
              };
              Services.obs.addObserver(obs, "browser-delayed-startup-finished");
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

              if (createProperties.openInReaderMode) {
                url = `about:reader?url=${encodeURIComponent(url)}`;
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
            let currentTab = window.gBrowser.selectedTab;

            if (createProperties.openerTabId !== null) {
              options.ownerTab = tabTracker.getTab(createProperties.openerTabId);
              options.openerBrowser = options.ownerTab.linkedBrowser;
              if (options.ownerTab.ownerGlobal !== window) {
                return Promise.reject({message: "Opener tab must be in the same window as the tab being created"});
              }
            }

            let nativeTab = window.gBrowser.addTab(url || window.BROWSER_NEW_TAB_URL, options);

            let active = true;
            if (createProperties.active !== null) {
              active = createProperties.active;
            }
            if (active) {
              window.gBrowser.selectedTab = nativeTab;
            }

            if (createProperties.index !== null) {
              window.gBrowser.moveTabTo(nativeTab, createProperties.index);
            }

            if (createProperties.pinned) {
              window.gBrowser.pinTab(nativeTab);
            }

            if (active && !url) {
              window.focusAndSelectUrlBar();
            }

            if (createProperties.url && createProperties.url !== window.BROWSER_NEW_TAB_URL) {
              // We can't wait for a location change event for about:newtab,
              // since it may be pre-rendered, in which case its initial
              // location change event has already fired.

              // Mark the tab as initializing, so that operations like
              // `executeScript` wait until the requested URL is loaded in
              // the tab before dispatching messages to the inner window
              // that contains the URL we're attempting to load.
              tabListener.initializingTabs.add(nativeTab);
            }

            return tabManager.convert(nativeTab, currentTab);
          });
        },

        async remove(tabs) {
          if (!Array.isArray(tabs)) {
            tabs = [tabs];
          }

          for (let tabId of tabs) {
            let nativeTab = tabTracker.getTab(tabId);
            nativeTab.ownerGlobal.gBrowser.removeTab(nativeTab);
          }
        },

        async discard(tabIds) {
          if (!Array.isArray(tabIds)) {
            tabIds = [tabIds];
          }
          let tabs = tabIds.map(tabId => tabTracker.getTab(tabId));

          for (let tab of tabs) {
            tab.ownerGlobal.gBrowser.discardBrowser(tab.linkedBrowser);
          }
        },

        async update(tabId, updateProperties) {
          let nativeTab = getTabOrActive(tabId);

          let tabbrowser = nativeTab.ownerGlobal.gBrowser;

          if (updateProperties.url !== null) {
            let url = context.uri.resolve(updateProperties.url);

            if (!context.checkLoadURL(url, {dontReportErrors: true})) {
              return Promise.reject({message: `Illegal URL: ${url}`});
            }

            let flags = updateProperties.loadReplace
              ? Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY
              : Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
            nativeTab.linkedBrowser.loadURIWithFlags(url, {flags});
          }

          if (updateProperties.active !== null) {
            if (updateProperties.active) {
              tabbrowser.selectedTab = nativeTab;
            } else {
              // Not sure what to do here? Which tab should we select?
            }
          }
          if (updateProperties.muted !== null) {
            if (nativeTab.muted != updateProperties.muted) {
              nativeTab.toggleMuteAudio(extension.id);
            }
          }
          if (updateProperties.pinned !== null) {
            if (updateProperties.pinned) {
              tabbrowser.pinTab(nativeTab);
            } else {
              tabbrowser.unpinTab(nativeTab);
            }
          }
          if (updateProperties.openerTabId !== null) {
            let opener = tabTracker.getTab(updateProperties.openerTabId);
            if (opener.ownerDocument !== nativeTab.ownerDocument) {
              return Promise.reject({message: "Opener tab must be in the same window as the tab being updated"});
            }
            tabTracker.setOpener(nativeTab, opener);
          }

          return tabManager.convert(nativeTab);
        },

        async reload(tabId, reloadProperties) {
          let nativeTab = getTabOrActive(tabId);

          let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
          if (reloadProperties && reloadProperties.bypassCache) {
            flags |= Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
          }
          nativeTab.linkedBrowser.reloadWithFlags(flags);
        },

        async get(tabId) {
          return tabManager.get(tabId).convert();
        },

        getCurrent() {
          let tabData;
          if (context.tabId) {
            tabData = tabManager.get(context.tabId).convert();
          }
          return Promise.resolve(tabData);
        },

        async query(queryInfo) {
          if (!extension.hasPermission("tabs")) {
            if (queryInfo.url !== null || queryInfo.title !== null) {
              return Promise.reject({message: 'The "tabs" permission is required to use the query API with the "url" or "title" parameters'});
            }
          }

          queryInfo = Object.assign({}, queryInfo);

          if (queryInfo.url !== null) {
            queryInfo.url = new MatchPatternSet([].concat(queryInfo.url));
          }
          if (queryInfo.title !== null) {
            queryInfo.title = new MatchGlob(queryInfo.title);
          }

          return Array.from(tabManager.query(queryInfo, context),
                            tab => tab.convert());
        },

        async captureTab(tabId, options) {
          let nativeTab = getTabOrActive(tabId);
          await tabListener.awaitTabReady(nativeTab);

          let tab = tabManager.wrapTab(nativeTab);
          return tab.capture(context, options);
        },

        async captureVisibleTab(windowId, options) {
          let window = windowId == null ?
            windowTracker.topWindow :
            windowTracker.getWindow(windowId, context);

          let tab = tabManager.wrapTab(window.gBrowser.selectedTab);
          await tabListener.awaitTabReady(tab.nativeTab);

          return tab.capture(context, options);
        },

        async detectLanguage(tabId) {
          let tab = await promiseTabWhenReady(tabId);

          return tab.sendMessage(context, "Extension:DetectLanguage");
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
          let lastInsertion = new Map();

          let tabs = tabIds.map(tabId => tabTracker.getTab(tabId));
          for (let nativeTab of tabs) {
            // If the window is not specified, use the window from the tab.
            let window = destinationWindow || nativeTab.ownerGlobal;
            let gBrowser = window.gBrowser;

            // If we are not moving the tab to a different window, and the window
            // only has one tab, do nothing.
            if (nativeTab.ownerGlobal == window && gBrowser.tabs.length === 1) {
              continue;
            }

            let insertionPoint = indexMap.get(window) || moveProperties.index;
            // If the index is -1 it should go to the end of the tabs.
            if (insertionPoint == -1) {
              insertionPoint = gBrowser.tabs.length;
            }

            // We can only move pinned tabs to a point within, or just after,
            // the current set of pinned tabs. Unpinned tabs, likewise, can only
            // be moved to a position after the current set of pinned tabs.
            // Attempts to move a tab to an illegal position are ignored.
            let numPinned = gBrowser._numPinnedTabs;
            let ok = nativeTab.pinned ? insertionPoint <= numPinned : insertionPoint >= numPinned;
            if (!ok) {
              continue;
            }

            // If this is not the first tab to be inserted into this window and
            // the insertion point is the same as the last insertion and
            // the tab is further to the right than the current insertion point
            // then you need to bump up the insertion point. See bug 1323311.
            if (lastInsertion.has(window) &&
                lastInsertion.get(window) === insertionPoint &&
                nativeTab._tPos > insertionPoint) {
              insertionPoint++;
              indexMap.set(window, insertionPoint);
            }

            if (nativeTab.ownerGlobal != window) {
              // If the window we are moving the tab in is different, then move the tab
              // to the new window.
              nativeTab = gBrowser.adoptTab(nativeTab, insertionPoint, false);
            } else {
              // If the window we are moving is the same, just move the tab.
              gBrowser.moveTabTo(nativeTab, insertionPoint);
            }
            lastInsertion.set(window, nativeTab._tPos);
            tabsMoved.push(nativeTab);
          }

          return tabsMoved.map(nativeTab => tabManager.convert(nativeTab));
        },

        duplicate(tabId) {
          let nativeTab = tabTracker.getTab(tabId);

          let gBrowser = nativeTab.ownerGlobal.gBrowser;
          let newTab = gBrowser.duplicateTab(nativeTab);

          return new Promise(resolve => {
            // We need to use SSTabRestoring because any attributes set before
            // are ignored. SSTabRestored is too late and results in a jump in
            // the UI. See http://bit.ly/session-store-api for more information.
            newTab.addEventListener("SSTabRestoring", function() {
              // As the tab is restoring, move it to the correct position.

              // Pinned tabs that are duplicated are inserted
              // after the existing pinned tab and pinned.
              if (nativeTab.pinned) {
                gBrowser.pinTab(newTab);
              }
              gBrowser.moveTabTo(newTab, nativeTab._tPos + 1);
            }, {once: true});

            newTab.addEventListener("SSTabRestored", function() {
              // Once it has been restored, select it and return the promise.
              gBrowser.selectedTab = newTab;

              resolve(tabManager.convert(newTab));
            }, {once: true});
          });
        },

        getZoom(tabId) {
          let nativeTab = getTabOrActive(tabId);

          let {ZoomManager} = nativeTab.ownerGlobal;
          let zoom = ZoomManager.getZoomForBrowser(nativeTab.linkedBrowser);

          return Promise.resolve(zoom);
        },

        setZoom(tabId, zoom) {
          let nativeTab = getTabOrActive(tabId);

          let {FullZoom, ZoomManager} = nativeTab.ownerGlobal;

          if (zoom === 0) {
            // A value of zero means use the default zoom factor.
            return FullZoom.reset(nativeTab.linkedBrowser);
          } else if (zoom >= ZoomManager.MIN && zoom <= ZoomManager.MAX) {
            FullZoom.setZoom(zoom, nativeTab.linkedBrowser);
          } else {
            return Promise.reject({
              message: `Zoom value ${zoom} out of range (must be between ${ZoomManager.MIN} and ${ZoomManager.MAX})`,
            });
          }

          return Promise.resolve();
        },

        _getZoomSettings(tabId) {
          let nativeTab = getTabOrActive(tabId);

          let {FullZoom} = nativeTab.ownerGlobal;

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
          let nativeTab = getTabOrActive(tabId);

          let currentSettings = this._getZoomSettings(tabTracker.getId(nativeTab));

          if (!Object.keys(settings).every(key => settings[key] === currentSettings[key])) {
            return Promise.reject(`Unsupported zoom settings: ${JSON.stringify(settings)}`);
          }
          return Promise.resolve();
        },

        onZoomChange: new EventManager(context, "tabs.onZoomChange", fire => {
          let getZoomLevel = browser => {
            let {ZoomManager} = browser.ownerGlobal;

            return ZoomManager.getZoomForBrowser(browser);
          };

          // Stores the last known zoom level for each tab's browser.
          // WeakMap[<browser> -> number]
          let zoomLevels = new WeakMap();

          // Store the zoom level for all existing tabs.
          for (let window of windowTracker.browserWindows()) {
            for (let nativeTab of window.gBrowser.tabs) {
              let browser = nativeTab.linkedBrowser;
              zoomLevels.set(browser, getZoomLevel(browser));
            }
          }

          let tabCreated = (eventName, event) => {
            let browser = event.nativeTab.linkedBrowser;
            zoomLevels.set(browser, getZoomLevel(browser));
          };


          let zoomListener = event => {
            let browser = event.originalTarget;

            // For non-remote browsers, this event is dispatched on the document
            // rather than on the <browser>.
            if (browser instanceof Ci.nsIDOMDocument) {
              browser = browser.docShell.chromeEventHandler;
            }

            let {gBrowser} = browser.ownerGlobal;
            let nativeTab = gBrowser.getTabForBrowser(browser);
            if (!nativeTab) {
              // We only care about zoom events in the top-level browser of a tab.
              return;
            }

            let oldZoomFactor = zoomLevels.get(browser);
            let newZoomFactor = getZoomLevel(browser);

            if (oldZoomFactor != newZoomFactor) {
              zoomLevels.set(browser, newZoomFactor);

              let tabId = tabTracker.getId(nativeTab);
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

        print() {
          let activeTab = getTabOrActive(null);
          let {PrintUtils} = activeTab.ownerGlobal;

          PrintUtils.printWindow(activeTab.linkedBrowser.outerWindowID, activeTab.linkedBrowser);
        },

        printPreview() {
          let activeTab = getTabOrActive(null);
          let {
            PrintUtils,
            PrintPreviewListener,
          } = activeTab.ownerGlobal;

          return new Promise((resolve, reject) => {
            let ppBrowser = PrintUtils._shouldSimplify ?
              PrintPreviewListener.getSimplifiedPrintPreviewBrowser() :
              PrintPreviewListener.getPrintPreviewBrowser();

            let mm = ppBrowser.messageManager;

            let onEntered = (message) => {
              mm.removeMessageListener("Printing:Preview:Entered", onEntered);
              if (message.data.failed) {
                reject({message: "Print preview failed"});
              }
              resolve();
            };

            mm.addMessageListener("Printing:Preview:Entered", onEntered);

            PrintUtils.printPreview(PrintPreviewListener);
          });
        },

        saveAsPDF(pageSettings) {
          let activeTab = getTabOrActive(null);
          let picker = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
          let title = strBundle.GetStringFromName("saveaspdf.saveasdialog.title");

          if (AppConstants.platform === "macosx") {
            return Promise.reject({message: "Not supported on Mac OS X"});
          }

          picker.init(activeTab.ownerGlobal, title, Ci.nsIFilePicker.modeSave);
          picker.appendFilter("PDF", "*.pdf");
          picker.defaultExtension = "pdf";
          picker.defaultString = activeTab.linkedBrowser.contentTitle + ".pdf";

          return new Promise(resolve => {
            picker.open(function(retval) {
              if (retval == 0 || retval == 2) {
                // OK clicked (retval == 0) or replace confirmed (retval == 2)
                try {
                  let fstream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(Ci.nsIFileOutputStream);
                  fstream.init(picker.file, 0x2A, 0x1B6, 0); // write|create|truncate, file permissions rw-rw-rw- = 0666 = 0x1B6
                  fstream.close(); // unlock file
                } catch (e) {
                  resolve(retval == 0 ? "not_saved" : "not_replaced");
                  return;
                }

                let psService = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(Ci.nsIPrintSettingsService);
                let printSettings = psService.newPrintSettings;

                printSettings.printToFile = true;
                printSettings.toFileName = picker.file.path;

                printSettings.printSilent = true;
                printSettings.showPrintProgress = false;

                printSettings.printFrameType = Ci.nsIPrintSettings.kFramesAsIs;
                printSettings.outputFormat = Ci.nsIPrintSettings.kOutputFormatPDF;

                if (pageSettings.orientation !== null) {
                  printSettings.orientation = pageSettings.orientation;
                }
                if (pageSettings.scaling !== null) {
                  printSettings.scaling = pageSettings.scaling;
                }
                if (pageSettings.shrinkToFit !== null) {
                  printSettings.shrinkToFit = pageSettings.shrinkToFit;
                }
                if (pageSettings.showBackgroundColors !== null) {
                  printSettings.printBGColors = pageSettings.showBackgroundColors;
                }
                if (pageSettings.showBackgroundImages !== null) {
                  printSettings.printBGImages = pageSettings.showBackgroundImages;
                }
                if (pageSettings.paperSizeUnit !== null) {
                  printSettings.paperSizeUnit = pageSettings.paperSizeUnit;
                }
                if (pageSettings.paperWidth !== null) {
                  printSettings.paperWidth = pageSettings.paperWidth;
                }
                if (pageSettings.paperHeight !== null) {
                  printSettings.paperHeight = pageSettings.paperHeight;
                }
                if (pageSettings.headerLeft !== null) {
                  printSettings.headerStrLeft = pageSettings.headerLeft;
                }
                if (pageSettings.headerCenter !== null) {
                  printSettings.headerStrCenter = pageSettings.headerCenter;
                }
                if (pageSettings.headerRight !== null) {
                  printSettings.headerStrRight = pageSettings.headerRight;
                }
                if (pageSettings.footerLeft !== null) {
                  printSettings.footerStrLeft = pageSettings.footerLeft;
                }
                if (pageSettings.footerCenter !== null) {
                  printSettings.footerStrCenter = pageSettings.footerCenter;
                }
                if (pageSettings.footerRight !== null) {
                  printSettings.footerStrRight = pageSettings.footerRight;
                }
                if (pageSettings.marginLeft !== null) {
                  printSettings.marginLeft = pageSettings.marginLeft;
                }
                if (pageSettings.marginRight !== null) {
                  printSettings.marginRight = pageSettings.marginRight;
                }
                if (pageSettings.marginTop !== null) {
                  printSettings.marginTop = pageSettings.marginTop;
                }
                if (pageSettings.marginBottom !== null) {
                  printSettings.marginBottom = pageSettings.marginBottom;
                }

                activeTab.linkedBrowser.print(activeTab.linkedBrowser.outerWindowID, printSettings, null);

                resolve(retval == 0 ? "saved" : "replaced");
              } else {
                // Cancel clicked (retval == 1)
                resolve("canceled");
              }
            });
          });
        },

        async toggleReaderMode(tabId) {
          let tab = await promiseTabWhenReady(tabId);
          if (!tab.isInReaderMode && !tab.isArticle) {
            throw new ExtensionError("The specified tab cannot be placed into reader mode.");
          }
          tab = getTabOrActive(tabId);

          tab.linkedBrowser.messageManager.sendAsyncMessage("Reader:ToggleReaderMode");
        },
      },
    };
    return self;
  }
};
