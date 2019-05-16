/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(this, "BrowserUtils",
                               "resource://gre/modules/BrowserUtils.jsm");
ChromeUtils.defineModuleGetter(this, "ExtensionControlledPopup",
                               "resource:///modules/ExtensionControlledPopup.jsm");
ChromeUtils.defineModuleGetter(this, "PrivateBrowsingUtils",
                               "resource://gre/modules/PrivateBrowsingUtils.jsm");
ChromeUtils.defineModuleGetter(this, "PromiseUtils",
                               "resource://gre/modules/PromiseUtils.jsm");
ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "SessionStore",
                               "resource:///modules/sessionstore/SessionStore.jsm");

XPCOMUtils.defineLazyGetter(this, "strBundle", function() {
  return Services.strings.createBundle("chrome://global/locale/extensions.properties");
});

var {
  ExtensionError,
} = ExtensionUtils;

const TABHIDE_PREFNAME = "extensions.webextensions.tabhide.enabled";
const MULTISELECT_PREFNAME = "browser.tabs.multiselect";
XPCOMUtils.defineLazyPreferenceGetter(this, "gMultiSelectEnabled", MULTISELECT_PREFNAME, false);

const TAB_HIDE_CONFIRMED_TYPE = "tabHideNotification";

const TAB_ID_NONE = -1;


XPCOMUtils.defineLazyGetter(this, "tabHidePopup", () => {
  return new ExtensionControlledPopup({
    confirmedType: TAB_HIDE_CONFIRMED_TYPE,
    anchorId: "alltabs-button",
    popupnotificationId: "extension-tab-hide-notification",
    descriptionId: "extension-tab-hide-notification-description",
    descriptionMessageId: "tabHideControlled.message",
    getLocalizedDescription: (doc, message, addonDetails) => {
      let image = doc.createXULElement("image");
      image.setAttribute("class", "extension-controlled-icon alltabs-icon");
      return BrowserUtils.getLocalizedFragment(doc, message, addonDetails, image);
    },
    learnMoreMessageId: "tabHideControlled.learnMore",
    learnMoreLink: "extension-hiding-tabs",
  });
});

function showHiddenTabs(id) {
  for (let win of Services.wm.getEnumerator("navigator:browser")) {
    if (win.closed || !win.gBrowser) {
      continue;
    }

    for (let tab of win.gBrowser.tabs) {
      if (tab.hidden && tab.ownerGlobal &&
          SessionStore.getCustomTabValue(tab, "hiddenBy") === id) {
        win.gBrowser.showTab(tab);
      }
    }
  }
}

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

const allAttrs = new Set(["attention", "audible", "favIconUrl", "mutedInfo", "sharingState", "title"]);
const allProperties = new Set([
  "attention",
  "audible",
  "discarded",
  "favIconUrl",
  "hidden",
  "isArticle",
  "mutedInfo",
  "pinned",
  "sharingState",
  "status",
  "title",
]);
const restricted = new Set(["url", "favIconUrl", "title"]);

class TabsUpdateFilterEventManager extends EventManager {
  constructor(context) {
    let {extension} = context;
    let {tabManager} = extension;

    let register = (fire, filterProps) => {
      let filter = {...filterProps};
      if (filter.urls) {
        filter.urls = new MatchPatternSet(filter.urls);
      }
      let needsModified = true;
      if (filter.properties) {
        // Default is to listen for all events.
        needsModified = filter.properties.some(p => allAttrs.has(p));
        filter.properties = new Set(filter.properties);
        // TODO Bug 1465520 remove warning when ready.
        if (filter.properties.has("isarticle")) {
          extension.logger.warn("The isarticle filter name is deprecated, use isArticle.");
          filter.properties.delete("isarticle");
          filter.properties.add("isArticle");
        }
      } else {
        filter.properties = allProperties;
      }

      function sanitize(extension, changeInfo) {
        let result = {};
        let nonempty = false;
        let hasTabs = extension.hasPermission("tabs");
        for (let prop in changeInfo) {
          if (hasTabs || !restricted.has(prop)) {
            nonempty = true;
            result[prop] = changeInfo[prop];
          }
        }
        return nonempty && result;
      }

      function getWindowID(windowId) {
        if (windowId === Window.WINDOW_ID_CURRENT) {
          let window = windowTracker.getTopWindow(context);
          if (!window) {
            return undefined;
          }
          return windowTracker.getId(window);
        }
        return windowId;
      }

      function matchFilters(tab, changed) {
        if (!filterProps) {
          return true;
        }
        if (filter.tabId != null && tab.id != filter.tabId) {
          return false;
        }
        if (filter.windowId != null && tab.windowId != getWindowID(filter.windowId)) {
          return false;
        }
        if (filter.urls) {
          // We check permission first because tab.uri is null if !hasTabPermission.
          return tab.hasTabPermission && filter.urls.matches(tab.uri);
        }
        return true;
      }

      let fireForTab = (tab, changed) => {
        // Tab may be null if private and not_allowed.
        if (!tab || !matchFilters(tab, changed)) {
          return;
        }

        let changeInfo = sanitize(extension, changed);
        if (changeInfo) {
          fire.async(tab.id, changeInfo, tab.convert());
        }
      };

      let listener = event => {
        if (!context.canAccessWindow(event.originalTarget.ownerGlobal)) {
          return;
        }
        let needed = [];
        if (event.type == "TabAttrModified") {
          let changed = event.detail.changed;
          if (changed.includes("image") && filter.properties.has("favIconUrl")) {
            needed.push("favIconUrl");
          }
          if (changed.includes("muted") && filter.properties.has("mutedInfo")) {
            needed.push("mutedInfo");
          }
          if (changed.includes("soundplaying") && filter.properties.has("audible")) {
            needed.push("audible");
          }
          if (changed.includes("label") && filter.properties.has("title")) {
            needed.push("title");
          }
          if (changed.includes("sharing") && filter.properties.has("sharingState")) {
            needed.push("sharingState");
          }
          if (changed.includes("attention") && filter.properties.has("attention")) {
            needed.push("attention");
          }
        } else if (event.type == "TabPinned") {
          needed.push("pinned");
        } else if (event.type == "TabUnpinned") {
          needed.push("pinned");
        } else if (event.type == "TabBrowserInserted") {
          // This may be an adopted tab. Bail early to avoid asking tabManager
          // about the tab before we run the adoption logic in ext-browser.js.
          if (event.detail.insertedOnTabCreation) {
            return;
          }
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
          if (!context.canAccessWindow(tabElem.ownerGlobal)) {
            return;
          }

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

        if (nativeTab && context.canAccessWindow(nativeTab.ownerGlobal)) {
          let tab = tabManager.getWrapper(nativeTab);
          fireForTab(tab, {isArticle: message.data.isArticle});
        }
      };

      let listeners = new Map();
      if (filter.properties.has("status")) {
        listeners.set("status", statusListener);
      }
      if (needsModified) {
        listeners.set("TabAttrModified", listener);
      }
      if (filter.properties.has("pinned")) {
        listeners.set("TabPinned", listener);
        listeners.set("TabUnpinned", listener);
      }
      if (filter.properties.has("discarded")) {
        listeners.set("TabBrowserInserted", listener);
        listeners.set("TabBrowserDiscarded", listener);
      }
      if (filter.properties.has("hidden")) {
        listeners.set("TabShow", listener);
        listeners.set("TabHide", listener);
      }

      for (let [name, listener] of listeners) {
        windowTracker.addListener(name, listener);
      }

      if (filter.properties.has("isArticle")) {
        tabTracker.on("tab-isarticle", isArticleChangeListener);
      }

      return () => {
        for (let [name, listener] of listeners) {
          windowTracker.removeListener(name, listener);
        }

        if (filter.properties.has("isArticle")) {
          tabTracker.off("tab-isarticle", isArticleChangeListener);
        }
      };
    };

    super({
      context,
      name: "tabs.onUpdated",
      register,
    });
  }

  addListener(callback, filter) {
    let {extension} = this.context;
    if (filter && filter.urls &&
        (!extension.hasPermission("tabs") && !extension.hasPermission("activeTab"))) {
      Cu.reportError("Url filtering in tabs.onUpdated requires \"tabs\" or \"activeTab\" permission.");
      return false;
    }
    return super.addListener(callback, filter);
  }
}

function TabEventManager({context, name, event, listener}) {
  let register = fire => {
    let listener2 = (eventName, eventData, ...args) => {
      if (!("isPrivate" in eventData)) {
        throw new Error(`isPrivate property missing in tabTracker event "${eventName}"`);
      }

      if (eventData.isPrivate && !context.privateBrowsingAllowed) {
        return;
      }

      listener(fire, eventData, ...args);
    };

    tabTracker.on(event, listener2);
    return () => {
      tabTracker.off(event, listener2);
    };
  };

  return new EventManager({context, name, register}).api();
}

this.tabs = class extends ExtensionAPI {
  static onUpdate(id, manifest) {
    if (!manifest.permissions || !manifest.permissions.includes("tabHide")) {
      showHiddenTabs(id);
    }
  }

  static onDisable(id) {
    showHiddenTabs(id);
    tabHidePopup.clearConfirmation(id);
  }

  static onUninstall(id) {
    tabHidePopup.clearConfirmation(id);
  }

  getAPI(context) {
    let {extension} = context;

    let {tabManager, windowManager} = extension;

    function getTabOrActive(tabId) {
      let tab = tabId !== null ? tabTracker.getTab(tabId) : tabTracker.activeTab;
      if (!context.canAccessWindow(tab.ownerGlobal)) {
        throw new ExtensionError(tabId === null ? "Cannot access activeTab" :
                                                  `Invalid tab ID: ${tabId}`);
      }
      return tab;
    }

    function getNativeTabsFromIDArray(tabIds) {
      if (!Array.isArray(tabIds)) {
        tabIds = [tabIds];
      }
      return tabIds.map(tabId => {
        let tab = tabTracker.getTab(tabId);
        if (!context.canAccessWindow(tab.ownerGlobal)) {
          throw new ExtensionError(`Invalid tab ID: ${tabId}`);
        }
        return tab;
      });
    }

    async function promiseTabWhenReady(tabId) {
      let tab;
      if (tabId !== null) {
        tab = tabManager.get(tabId);
      } else {
        tab = tabManager.getWrapper(tabTracker.activeTab);
      }
      if (!tab) {
        throw new ExtensionError(tabId == null ?
          "Cannot access activeTab" : `Invalid tab ID: ${tabId}`);
      }

      await tabListener.awaitTabReady(tab.nativeTab);

      return tab;
    }

    let self = {
      tabs: {
        onActivated: TabEventManager({
          context,
          name: "tabs.onActivated",
          event: "tab-activated",
          listener: (fire, event) => {
            let {tabId, windowId, previousTabId, previousTabIsPrivate} = event;
            if (previousTabIsPrivate && !context.privateBrowsingAllowed) {
              previousTabId = undefined;
            }
            fire.async({tabId, previousTabId, windowId});
          },
        }),

        onCreated: TabEventManager({
          context,
          name: "tabs.onCreated",
          event: "tab-created",
          listener: (fire, event) => {
            fire.async(tabManager.convert(event.nativeTab, event.currentTabSize));
          },
        }),

        onHighlighted: TabEventManager({
          context,
          name: "tabs.onHighlighted",
          event: "tabs-highlighted",
          listener: (fire, event) => {
            fire.async({tabIds: event.tabIds, windowId: event.windowId});
          },
        }),

        onAttached: TabEventManager({
          context,
          name: "tabs.onAttached",
          event: "tab-attached",
          listener: (fire, event) => {
            fire.async(event.tabId, {newWindowId: event.newWindowId, newPosition: event.newPosition});
          },
        }),

        onDetached: TabEventManager({
          context,
          name: "tabs.onDetached",
          event: "tab-detached",
          listener: (fire, event) => {
            fire.async(event.tabId, {oldWindowId: event.oldWindowId, oldPosition: event.oldPosition});
          },
        }),

        onRemoved: TabEventManager({
          context,
          name: "tabs.onRemoved",
          event: "tab-removed",
          listener: (fire, event) => {
            fire.async(event.tabId, {windowId: event.windowId, isWindowClosing: event.isWindowClosing});
          },
        }),

        onReplaced: new EventManager({
          context,
          name: "tabs.onReplaced",
          register: fire => {
            return () => {};
          },
        }).api(),

        onMoved: new EventManager({
          context,
          name: "tabs.onMoved",
          register: fire => {
            let moveListener = event => {
              let nativeTab = event.originalTarget;
              if (context.canAccessWindow(nativeTab.ownerGlobal)) {
                fire.async(tabTracker.getId(nativeTab), {
                  windowId: windowTracker.getId(nativeTab.ownerGlobal),
                  fromIndex: event.detail,
                  toIndex: nativeTab._tPos,
                });
              }
            };

            windowTracker.addListener("TabMove", moveListener);
            return () => {
              windowTracker.removeListener("TabMove", moveListener);
            };
          },
        }).api(),

        onUpdated: new TabsUpdateFilterEventManager(context).api(),

        create(createProperties) {
          return new Promise((resolve, reject) => {
            let window = createProperties.windowId !== null ?
              windowTracker.getWindow(createProperties.windowId, context) :
              windowTracker.getTopNormalWindow(context);
            if (!window || !context.canAccessWindow(window)) {
              throw new Error("Not allowed to create tabs on the target window");
            }
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
            let principal = context.principal;

            let options = {};
            if (createProperties.cookieStoreId) {
              // May throw if validation fails.
              options.userContextId = getUserContextIdForCookieStoreId(extension, createProperties.cookieStoreId, PrivateBrowsingUtils.isBrowserPrivate(window.gBrowser));
            }

            if (createProperties.url !== null) {
              url = context.uri.resolve(createProperties.url);

              if (!context.checkLoadURL(url, {dontReportErrors: true})) {
                return Promise.reject({message: `Illegal URL: ${url}`});
              }

              if (createProperties.openInReaderMode) {
                url = `about:reader?url=${encodeURIComponent(url)}`;
              }
            } else {
              url = window.BROWSER_NEW_TAB_URL;
            }
            // Only set allowInheritPrincipal on discardable urls as it
            // will override creating a lazy browser.  Setting triggeringPrincipal
            // will ensure other cases are handled, but setting it may prevent
            // creating about and data urls.
            let discardable = url && !url.startsWith("about:");
            if (!discardable) {
              // Make sure things like about:blank and data: URIs never inherit,
              // and instead always get a NullPrincipal.
              options.allowInheritPrincipal = false;
              // Falling back to codebase here as about: requires it, however is safe.
              principal = Services.scriptSecurityManager.createCodebasePrincipal(Services.io.newURI(url), {
                userContextId: options.userContextId,
                privateBrowsingId: PrivateBrowsingUtils.isBrowserPrivate(window.gBrowser) ? 1 : 0,
              });
            } else {
              options.allowInheritPrincipal = true;
              options.triggeringPrincipal = context.principal;
            }

            tabListener.initTabReady();
            const currentTab = window.gBrowser.selectedTab;
            const {frameLoader} = currentTab.linkedBrowser;
            const currentTabSize = {
              width: frameLoader.lazyWidth,
              height: frameLoader.lazyHeight,
            };

            if (createProperties.openerTabId !== null) {
              options.ownerTab = tabTracker.getTab(createProperties.openerTabId);
              options.openerBrowser = options.ownerTab.linkedBrowser;
              if (options.ownerTab.ownerGlobal !== window) {
                return Promise.reject({message: "Opener tab must be in the same window as the tab being created"});
              }
            }

            // Simple properties
            const properties = ["index", "pinned"];
            for (let prop of properties) {
              if (createProperties[prop] != null) {
                options[prop] = createProperties[prop];
              }
            }

            let active = createProperties.active !== null ?
                         createProperties.active : !createProperties.discarded;
            if (createProperties.discarded) {
              if (active) {
                return Promise.reject({message: `Active tabs cannot be created and discarded.`});
              }
              if (createProperties.pinned) {
                return Promise.reject({message: `Pinned tabs cannot be created and discarded.`});
              }
              if (!discardable) {
                return Promise.reject({message: `Cannot create a discarded new tab or "about" urls.`});
              }
              options.createLazyBrowser = true;
              options.lazyTabTitle = createProperties.title;
            } else if (createProperties.title) {
              return Promise.reject({message: `Title may only be set for discarded tabs.`});
            }

            options.triggeringPrincipal = principal;
            let nativeTab = window.gBrowser.addTab(url, options);

            if (active) {
              window.gBrowser.selectedTab = nativeTab;
              if (!createProperties.url) {
                window.focusAndSelectUrlBar();
              }
            }

            if (createProperties.url &&
                createProperties.url !== window.BROWSER_NEW_TAB_URL) {
              // We can't wait for a location change event for about:newtab,
              // since it may be pre-rendered, in which case its initial
              // location change event has already fired.

              // Mark the tab as initializing, so that operations like
              // `executeScript` wait until the requested URL is loaded in
              // the tab before dispatching messages to the inner window
              // that contains the URL we're attempting to load.
              tabListener.initializingTabs.add(nativeTab);
            }

            return tabManager.convert(nativeTab, currentTabSize);
          });
        },

        async remove(tabIds) {
          for (let nativeTab of getNativeTabsFromIDArray(tabIds)) {
            nativeTab.ownerGlobal.gBrowser.removeTab(nativeTab);
          }
        },

        async discard(tabIds) {
          for (let nativeTab of getNativeTabsFromIDArray(tabIds)) {
            nativeTab.ownerGlobal.gBrowser.discardBrowser(nativeTab);
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

            let options = {
              flags: updateProperties.loadReplace
                      ? Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY
                      : Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
              triggeringPrincipal: context.principal,
            };
            nativeTab.linkedBrowser.loadURI(url, options);
          }

          if (updateProperties.active !== null) {
            if (updateProperties.active) {
              tabbrowser.selectedTab = nativeTab;
            } else {
              // Not sure what to do here? Which tab should we select?
            }
          }
          if (updateProperties.highlighted !== null) {
            if (!gMultiSelectEnabled) {
              throw new ExtensionError(`updateProperties.highlight is currently experimental and must be enabled with the ${MULTISELECT_PREFNAME} preference.`);
            }
            if (updateProperties.highlighted) {
              if (!nativeTab.selected && !nativeTab.multiselected) {
                tabbrowser.addToMultiSelectedTabs(nativeTab, false);
                // Select the highlighted tab unless active:false is provided.
                // Note that Chrome selects it even in that case.
                if (updateProperties.active !== false) {
                  tabbrowser.lockClearMultiSelectionOnce();
                  tabbrowser.selectedTab = nativeTab;
                }
              }
            } else {
              tabbrowser.removeFromMultiSelectedTabs(nativeTab, true);
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
          if (updateProperties.successorTabId !== null) {
            let successor = null;
            if (updateProperties.successorTabId !== TAB_ID_NONE) {
              successor = tabTracker.getTab(updateProperties.successorTabId, null);
              if (!successor) {
                throw new ExtensionError("Invalid successorTabId");
              }
              // This also ensures "privateness" matches.
              if (successor.ownerDocument !== nativeTab.ownerDocument) {
                throw new ExtensionError("Successor tab must be in the same window as the tab being updated");
              }
            }
            tabbrowser.setSuccessor(nativeTab, successor);
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
            windowTracker.getTopWindow(context) :
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
            destinationWindow = windowTracker.getWindow(moveProperties.windowId, context);
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

          for (let nativeTab of getNativeTabsFromIDArray(tabIds)) {
            // If the window is not specified, use the window from the tab.
            let window = destinationWindow || nativeTab.ownerGlobal;
            let gBrowser = window.gBrowser;

            // If we are not moving the tab to a different window, and the window
            // only has one tab, do nothing.
            if (nativeTab.ownerGlobal == window && gBrowser.tabs.length === 1) {
              continue;
            }
            // If moving between windows, be sure privacy matches.  While gBrowser
            // prevents this, we want to silently ignore it.
            if (nativeTab.ownerGlobal != window &&
                PrivateBrowsingUtils.isBrowserPrivate(window.gBrowser) !=
                PrivateBrowsingUtils.isBrowserPrivate(nativeTab.ownerGlobal.gBrowser)) {
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
          // Schema requires tab id.
          let nativeTab = getTabOrActive(tabId);

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

        onZoomChange: new EventManager({
          context,
          name: "tabs.onZoomChange",
          register: fire => {
            let getZoomLevel = browser => {
              let {ZoomManager} = browser.ownerGlobal;

              return ZoomManager.getZoomForBrowser(browser);
            };

            // Stores the last known zoom level for each tab's browser.
            // WeakMap[<browser> -> number]
            let zoomLevels = new WeakMap();

            // Store the zoom level for all existing tabs.
            for (let window of windowTracker.browserWindows()) {
              if (!context.canAccessWindow(window)) {
                continue;
              }
              for (let nativeTab of window.gBrowser.tabs) {
                let browser = nativeTab.linkedBrowser;
                zoomLevels.set(browser, getZoomLevel(browser));
              }
            }

            let tabCreated = (eventName, event) => {
              let browser = event.nativeTab.linkedBrowser;
              if (!event.isPrivate || context.privateBrowsingAllowed) {
                zoomLevels.set(browser, getZoomLevel(browser));
              }
            };


            let zoomListener = event => {
              let browser = event.originalTarget;

              // For non-remote browsers, this event is dispatched on the document
              // rather than on the <browser>.  But either way we have a node here.
              if (browser.nodeType == browser.DOCUMENT_NODE) {
                browser = browser.docShell.chromeEventHandler;
              }

              if (!context.canAccessWindow(browser.ownerGlobal)) {
                return;
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
          },
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
            let ppBrowser = PrintUtils.shouldSimplify ?
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

                // Workaround: When trying to replace an existing file that is open in another application (i.e. a locked file),
                // the print progress listener is never called. This workaround ensures that a correct status is always returned.
                try {
                  let fstream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(Ci.nsIFileOutputStream);
                  fstream.init(picker.file, 0x2A, 0o666, 0); // ioflags = write|create|truncate, file permissions = rw-rw-rw-
                  fstream.close();
                } catch (e) {
                  resolve(retval == 0 ? "not_saved" : "not_replaced");
                  return;
                }

                let psService = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(Ci.nsIPrintSettingsService);
                let printSettings = psService.newPrintSettings;

                printSettings.printerName = "";
                printSettings.isInitializedFromPrinter = true;
                printSettings.isInitializedFromPrefs = true;

                printSettings.printToFile = true;
                printSettings.toFileName = picker.file.path;

                printSettings.printSilent = true;
                printSettings.showPrintProgress = false;

                printSettings.printFrameType = Ci.nsIPrintSettings.kFramesAsIs;
                printSettings.outputFormat = Ci.nsIPrintSettings.kOutputFormatPDF;

                if (pageSettings.paperSizeUnit !== null) {
                  printSettings.paperSizeUnit = pageSettings.paperSizeUnit;
                }
                if (pageSettings.paperWidth !== null) {
                  printSettings.paperWidth = pageSettings.paperWidth;
                }
                if (pageSettings.paperHeight !== null) {
                  printSettings.paperHeight = pageSettings.paperHeight;
                }
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
                if (pageSettings.edgeLeft !== null) {
                  printSettings.edgeLeft = pageSettings.edgeLeft;
                }
                if (pageSettings.edgeRight !== null) {
                  printSettings.edgeRight = pageSettings.edgeRight;
                }
                if (pageSettings.edgeTop !== null) {
                  printSettings.edgeTop = pageSettings.edgeTop;
                }
                if (pageSettings.edgeBottom !== null) {
                  printSettings.edgeBottom = pageSettings.edgeBottom;
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

                let printProgressListener = {
                  onLocationChange(webProgress, request, location, flags) { },
                  onProgressChange(webProgress, request, curSelfProgress, maxSelfProgress, curTotalProgress, maxTotalProgress) { },
                  onSecurityChange(webProgress, request, state) { },
                  onContentBlockingEvent(webProgress, request, event) { },
                  onStateChange(webProgress, request, flags, status) {
                    if ((flags & Ci.nsIWebProgressListener.STATE_STOP) && (flags & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT)) {
                      resolve(retval == 0 ? "saved" : "replaced");
                    }
                  },
                  onStatusChange: function(webProgress, request, status, message) {
                    if (status != 0) {
                      resolve(retval == 0 ? "not_saved" : "not_replaced");
                    }
                  },
                  QueryInterface: ChromeUtils.generateQI([Ci.nsIWebProgressListener]),
                };

                activeTab.linkedBrowser.print(activeTab.linkedBrowser.outerWindowID, printSettings, printProgressListener);
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
          let nativeTab = getTabOrActive(tabId);
          nativeTab.linkedBrowser.messageManager.sendAsyncMessage("Reader:ToggleReaderMode");
        },

        moveInSuccession(tabIds, tabId, options) {
          const {insert, append} = options || {};
          const tabIdSet = new Set(tabIds);
          if (tabIdSet.size !== tabIds.length) {
            throw new ExtensionError("IDs must not occur more than once in tabIds");
          }
          if ((append || insert) && tabIdSet.has(tabId)) {
            throw new ExtensionError("Value of tabId must not occur in tabIds if append or insert is true");
          }

          const referenceTab = tabTracker.getTab(tabId, null);
          let referenceWindow = referenceTab && referenceTab.ownerGlobal;
          if (referenceWindow && !context.canAccessWindow(referenceWindow)) {
            throw new ExtensionError(`Invalid tab ID: ${tabId}`);
          }
          let previousTab, lastSuccessor;
          if (append) {
            previousTab = referenceTab;
            lastSuccessor = (insert && referenceTab && referenceTab.successor) || null;
          } else {
            lastSuccessor = referenceTab;
          }

          let firstTab;
          for (const tabId of tabIds) {
            const tab = tabTracker.getTab(tabId, null);
            if (tab === null) {
              continue;
            }
            if (!context.canAccessWindow(tab.ownerGlobal)) {
              throw new ExtensionError(`Invalid tab ID: ${tabId}`);
            }
            if (referenceWindow === null) {
              referenceWindow = tab.ownerGlobal;
            } else if (tab.ownerGlobal !== referenceWindow) {
              continue;
            }
            referenceWindow.gBrowser.replaceInSuccession(tab, tab.successor);
            if (append && tab === lastSuccessor) {
              lastSuccessor = tab.successor;
            }
            if (previousTab) {
              referenceWindow.gBrowser.setSuccessor(previousTab, tab);
            } else {
              firstTab = tab;
            }
            previousTab = tab;
          }

          if (previousTab) {
            if (!append && insert && lastSuccessor !== null) {
              referenceWindow.gBrowser.replaceInSuccession(lastSuccessor, firstTab);
            }
            referenceWindow.gBrowser.setSuccessor(previousTab, lastSuccessor);
          }
        },

        show(tabIds) {
          if (!Services.prefs.getBoolPref(TABHIDE_PREFNAME, false)) {
            throw new ExtensionError(`tabs.show is currently experimental and must be enabled with the ${TABHIDE_PREFNAME} preference.`);
          }

          for (let tab of getNativeTabsFromIDArray(tabIds)) {
            if (tab.ownerGlobal) {
              tab.ownerGlobal.gBrowser.showTab(tab);
            }
          }
        },

        hide(tabIds) {
          if (!Services.prefs.getBoolPref(TABHIDE_PREFNAME, false)) {
            throw new ExtensionError(`tabs.hide is currently experimental and must be enabled with the ${TABHIDE_PREFNAME} preference.`);
          }

          let hidden = [];
          for (let tab of getNativeTabsFromIDArray(tabIds)) {
            if (tab.ownerGlobal && !tab.hidden) {
              tab.ownerGlobal.gBrowser.hideTab(tab, extension.id);
              if (tab.hidden) {
                hidden.push(tabTracker.getId(tab));
              }
            }
          }
          if (hidden.length > 0) {
            let win = Services.wm.getMostRecentWindow("navigator:browser");
            tabHidePopup.open(win, extension.id);
          }
          return hidden;
        },

        highlight(highlightInfo) {
          if (!gMultiSelectEnabled) {
            throw new ExtensionError(`tabs.highlight is currently experimental and must be enabled with the ${MULTISELECT_PREFNAME} preference.`);
          }
          let {windowId, tabs, populate} = highlightInfo;
          if (windowId == null) {
            windowId = Window.WINDOW_ID_CURRENT;
          }
          let window = windowTracker.getWindow(windowId, context);
          if (!context.canAccessWindow(window)) {
            throw new ExtensionError(`Invalid window ID: ${windowId}`);
          }

          if (!Array.isArray(tabs)) {
            tabs = [tabs];
          } else if (tabs.length == 0) {
            throw new ExtensionError("No highlighted tab.");
          }
          window.gBrowser.selectedTabs = tabs.map((tabIndex) => {
            let tab = window.gBrowser.tabs[tabIndex];
            if (!tab) {
              throw new ExtensionError("No tab at index: " + tabIndex);
            }
            return tab;
          });
          return windowManager.convert(window, {populate});
        },
      },
    };
    return self;
  }
};
