/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PromiseUtils",
                                  "resource://gre/modules/PromiseUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

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
        onActivated: new SingletonEventManager(context, "tabs.onActivated", fire => {
          let listener = (eventName, event) => {
            fire.async(event);
          };

          tabTracker.on("tab-activated", listener);
          return () => {
            tabTracker.off("tab-activated", listener);
          };
        }).api(),

        onCreated: new SingletonEventManager(context, "tabs.onCreated", fire => {
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
        onHighlighted: new SingletonEventManager(context, "tabs.onHighlighted", fire => {
          let listener = (eventName, event) => {
            fire.async({tabIds: [event.tabId], windowId: event.windowId});
          };

          tabTracker.on("tab-activated", listener);
          return () => {
            tabTracker.off("tab-activated", listener);
          };
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

        onReplaced: new SingletonEventManager(context, "tabs.onReplaced", fire => {
          return () => {};
        }).api(),

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

        async update(tabId, updateProperties) {
          let nativeTab = getTabOrActive(tabId);

          let tabbrowser = nativeTab.ownerGlobal.gBrowser;

          if (updateProperties.url !== null) {
            let url = context.uri.resolve(updateProperties.url);

            if (!context.checkLoadURL(url, {dontReportErrors: true})) {
              return Promise.reject({message: `Illegal URL: ${url}`});
            }

            nativeTab.linkedBrowser.loadURI(url);
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
              nativeTab.toggleMuteAudio(extension.uuid);
            }
          }
          if (updateProperties.pinned !== null) {
            if (updateProperties.pinned) {
              tabbrowser.pinTab(nativeTab);
            } else {
              tabbrowser.unpinTab(nativeTab);
            }
          }
          // FIXME: highlighted/selected, openerTabId

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
          if (queryInfo.url !== null) {
            if (!extension.hasPermission("tabs")) {
              return Promise.reject({message: 'The "tabs" permission is required to use the query API with the "url" parameter'});
            }

            queryInfo = Object.assign({}, queryInfo);
            queryInfo.url = new MatchPatternSet([].concat(queryInfo.url));
          }

          return Array.from(tabManager.query(queryInfo, context),
                            tab => tab.convert());
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
              browser = browser.defaultView.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIDocShell)
                               .chromeEventHandler;
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
      },
    };
    return self;
  }
};
