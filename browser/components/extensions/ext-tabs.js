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
function getSender(context, target, sender)
{
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
  } else {
    // The message came from an ExtensionPage. In that case, it should
    // include a tabId property (which is filled in by the page-open
    // listener below).
    if ("tabId" in sender) {
      sender.tab = TabManager.convert(context.extension, TabManager.getTab(sender.tabId));
      delete sender.tabId;
    }
  }
}

// WeakMap[ExtensionPage -> {tab, parentWindow}]
var pageDataMap = new WeakMap();

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

global.currentWindow = function(context)
{
  let pageData = pageDataMap.get(context);
  if (pageData) {
    return pageData.parentWindow;
  }
  return WindowManager.topWindow;
}

// TODO: activeTab permission

extensions.registerAPI((extension, context) => {
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
          fire({tab: TabManager.convert(extension, tab)});
        };

        let windowListener = window => {
          for (let tab of window.gBrowser.tabs) {
            fire({tab: TabManager.convert(extension, tab)});
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

        let listener = event => {
          let tab = event.originalTarget;
          let window = tab.ownerDocument.defaultView;
          let tabId = TabManager.getId(tab);

          let changeInfo = {};
          let needed = false;
          if (event.type == "TabAttrModified") {
            if (event.detail.changed.indexOf("image") != -1) {
              changeInfo.favIconUrl = window.gBrowser.getIcon(tab);
              needed = true;
            }
          } else if (event.type == "TabPinned") {
            changeInfo.pinned = true;
            needed = true;
          } else if (event.type == "TabUnpinned") {
            changeInfo.pinned = false;
            needed = true;
          }

          [needed, changeInfo] = sanitize(extension, changeInfo);
          if (needed) {
            fire(tabId, changeInfo, TabManager.convert(extension, tab));
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

            let gBrowser = browser.ownerDocument.defaultView.gBrowser;
            let tab = gBrowser.getTabForBrowser(browser);
            let tabId = TabManager.getId(tab);
            let [needed, changeInfo] = sanitize(extension, {status});
            if (needed) {
              fire(tabId, changeInfo, TabManager.convert(extension, tab));
            }
          },

          onLocationChange(browser, webProgress, request, locationURI, flags) {
            if (!webProgress.isTopLevel) {
              return;
            }
            let gBrowser = browser.ownerDocument.defaultView.gBrowser;
            let tab = gBrowser.getTabForBrowser(browser);
            let tabId = TabManager.getId(tab);
            let [needed, changeInfo] = sanitize(extension, {
              status: webProgress.isLoadingDocument ? "loading" : "complete",
              url: locationURI.spec
            });
            if (needed) {
              fire(tabId, changeInfo, TabManager.convert(extension, tab));
            }
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

      onReplaced: ignoreEvent(),

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
        if (!createProperties) {
          createProperties = {};
        }

        let url = createProperties.url || aboutNewTabService.newTabURL;
        url = extension.baseURI.resolve(url);

        function createInWindow(window) {
          let tab = window.gBrowser.addTab(url);

          let active = true;
          if ("active" in createProperties) {
            active = createProperties.active;
          } else if ("selected" in createProperties) {
            active = createProperties.selected;
          }
          if (active) {
            window.gBrowser.selectedTab = tab;
          }

          if ("index" in createProperties) {
            window.gBrowser.moveTabTo(tab, createProperties.index);
          }

          if (createProperties.pinned) {
            window.gBrowser.pinTab(tab);
          }

          if (callback) {
            runSafe(context, callback, TabManager.convert(extension, tab));
          }
        }

        let window = "windowId" in createProperties ?
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

      update: function(...args) {
        let tabId, updateProperties, callback;
        if (args.length == 1) {
          updateProperties = args[0];
        } else {
          [tabId, updateProperties, callback] = args;
        }

        let tab = tabId ? TabManager.getTab(tabId) : TabManager.activeTab;
        let tabbrowser = tab.ownerDocument.defaultView.gBrowser;
        if ("url" in updateProperties) {
          tab.linkedBrowser.loadURI(updateProperties.url);
        }
        if ("active" in updateProperties) {
          if (updateProperties.active) {
            tabbrowser.selectedTab = tab;
          } else {
            // Not sure what to do here? Which tab should we select?
          }
        }
        if ("pinned" in updateProperties) {
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
        let tab = tabId ? TabManager.getTab(tabId) : TabManager.activeTab;
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

      getAllInWindow: function(...args) {
        let window, callback;
        if (args.length == 1) {
          callbacks = args[0];
        } else {
          window = WindowManager.getWindow(args[0]);
          callback = args[1];
        }

        if (!window) {
          window = WindowManager.topWindow;
        }

        return self.tabs.query({windowId: WindowManager.getId(window)}, callback);
      },

      query: function(queryInfo, callback) {
        if (!queryInfo) {
          queryInfo = {};
        }

        let pattern = null;
        if (queryInfo.url) {
          pattern = new MatchPattern(queryInfo.url);
        }

        function matches(window, tab) {
          let props = ["active", "pinned", "highlighted", "status", "title", "index"];
          for (let prop of props) {
            if (prop in queryInfo && queryInfo[prop] != tab[prop]) {
              return false;
            }
          }

          let lastFocused = window == WindowManager.topWindow;
          if ("lastFocusedWindow" in queryInfo && queryInfo.lastFocusedWindow != lastFocused) {
            return false;
          }

          let windowType = WindowManager.windowType(window);
          if ("windowType" in queryInfo && queryInfo.windowType != windowType) {
            return false;
          }

          if ("windowId" in queryInfo) {
            if (queryInfo.windowId == WindowManager.WINDOW_ID_CURRENT) {
              if (currentWindow(context) != window) {
                return false;
              }
            } else {
              if (queryInfo.windowId != tab.windowId) {
                return false;
              }
            }
          }

          if ("currentWindow" in queryInfo) {
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
        let e = Services.wm.getEnumerator("navigator:browser");
        while (e.hasMoreElements()) {
          let window = e.getNext();
          if (window.document.readyState != "complete") {
            continue;
          }
          let tabs = TabManager.getTabs(extension, window);
          for (let tab of tabs) {
            if (matches(window, tab)) {
              result.push(tab);
            }
          }
        }
        runSafe(context, callback, result);
      },

      _execute: function(tabId, details, kind, callback) {
        let tab = tabId ? TabManager.getTab(tabId) : TabManager.activeTab;
        let mm = tab.linkedBrowser.messageManager;

        let options = {js: [], css: []};
        if (details.code) {
          options[kind + 'Code'] = details.code;
        }
        if (details.file) {
          options[kind].push(extension.baseURI.resolve(details.file));
        }
        if (details.allFrames) {
          options.all_frames = details.allFrames;
        }
        if (details.matchAboutBlank) {
          options.match_about_blank = details.matchAboutBlank;
        }
        if (details.runAt) {
          options.run_at = details.runAt;
        }
        mm.sendAsyncMessage("Extension:Execute",
                            {extensionId: extension.id, options});

        // TODO: Call the callback with the result (which is what???).
      },

      executeScript: function(...args) {
        if (args.length == 1) {
          self.tabs._execute(undefined, args[0], 'js', undefined);
        } else {
          self.tabs._execute(args[0], args[1], 'js', args[2]);
        }
      },

      insertCss: function(tabId, details, callback) {
        if (args.length == 1) {
          self.tabs._execute(undefined, args[0], 'css', undefined);
        } else {
          self.tabs._execute(args[0], args[1], 'css', args[2]);
        }
      },

      connect: function(tabId, connectInfo) {
        let tab = TabManager.getTab(tabId);
        let mm = tab.linkedBrowser.messageManager;

        let name = connectInfo.name || "";
        let recipient = {extensionId: extension.id};
        if ("frameId" in connectInfo) {
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
        if (options && "frameId" in options) {
          recipient.frameId = options.frameId;
        }
        return context.messenger.sendMessage(mm, message, recipient, responseCallback);
      },
    },
  };
  return self;
});
