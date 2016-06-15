/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
} = ExtensionUtils;

extensions.registerSchemaAPI("windows", (extension, context) => {
  return {
    windows: {
      onCreated:
      new WindowEventManager(context, "windows.onCreated", "domwindowopened", (fire, window) => {
        fire(WindowManager.convert(extension, window));
      }).api(),

      onRemoved:
      new WindowEventManager(context, "windows.onRemoved", "domwindowclosed", (fire, window) => {
        fire(WindowManager.getId(window));
      }).api(),

      onFocusChanged: new EventManager(context, "windows.onFocusChanged", fire => {
        // Keep track of the last windowId used to fire an onFocusChanged event
        let lastOnFocusChangedWindowId;

        let listener = event => {
          let window = WindowManager.topWindow;
          let windowId = window ? WindowManager.getId(window) : WindowManager.WINDOW_ID_NONE;
          if (windowId !== lastOnFocusChangedWindowId) {
            fire(windowId);
            lastOnFocusChangedWindowId = windowId;
          }
        };
        AllWindowEvents.addListener("focus", listener);
        AllWindowEvents.addListener("blur", listener);
        return () => {
          AllWindowEvents.removeListener("focus", listener);
          AllWindowEvents.removeListener("blur", listener);
        };
      }).api(),

      get: function(windowId, getInfo) {
        let window = WindowManager.getWindow(windowId, context);
        return Promise.resolve(WindowManager.convert(extension, window, getInfo));
      },

      getCurrent: function(getInfo) {
        let window = currentWindow(context);
        return Promise.resolve(WindowManager.convert(extension, window, getInfo));
      },

      getLastFocused: function(getInfo) {
        let window = WindowManager.topWindow;
        return Promise.resolve(WindowManager.convert(extension, window, getInfo));
      },

      getAll: function(getInfo) {
        let windows = Array.from(WindowListManager.browserWindows(),
                                 window => WindowManager.convert(extension, window, getInfo));
        return Promise.resolve(windows);
      },

      create: function(createData) {
        let needResize = (createData.left !== null || createData.top !== null ||
                          createData.width !== null || createData.height !== null);

        if (needResize) {
          if (createData.state !== null && createData.state != "normal") {
            return Promise.reject({message: `"state": "${createData.state}" may not be combined with "left", "top", "width", or "height"`});
          }
          createData.state = "normal";
        }

        function mkstr(s) {
          let result = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
          result.data = s;
          return result;
        }

        let args = Cc["@mozilla.org/supports-array;1"].createInstance(Ci.nsISupportsArray);

        if (createData.tabId !== null) {
          if (createData.url !== null) {
            return Promise.reject({message: "`tabId` may not be used in conjunction with `url`"});
          }

          let tab = TabManager.getTab(createData.tabId);
          if (tab == null) {
            return Promise.reject({message: `Invalid tab ID: ${createData.tabId}`});
          }

          // Private browsing tabs can only be moved to private browsing
          // windows.
          let incognito = PrivateBrowsingUtils.isBrowserPrivate(tab.linkedBrowser);
          if (createData.incognito !== null && createData.incognito != incognito) {
            return Promise.reject({message: "`incognito` property must match the incognito state of tab"});
          }
          createData.incognito = incognito;

          args.AppendElement(tab);
        } else if (createData.url !== null) {
          if (Array.isArray(createData.url)) {
            let array = Cc["@mozilla.org/supports-array;1"].createInstance(Ci.nsISupportsArray);
            for (let url of createData.url) {
              array.AppendElement(mkstr(url));
            }
            args.AppendElement(array);
          } else {
            args.AppendElement(mkstr(createData.url));
          }
        } else {
          args.AppendElement(mkstr(aboutNewTabService.newTabURL));
        }

        let features = ["chrome"];

        if (createData.type === null || createData.type == "normal") {
          features.push("dialog=no", "all");
        } else {
          // All other types create "popup"-type windows by default.
          features.push("dialog", "resizable", "minimizable", "centerscreen", "titlebar", "close");
        }

        if (createData.incognito !== null) {
          if (createData.incognito) {
            features.push("private");
          } else {
            features.push("non-private");
          }
        }

        let window = Services.ww.openWindow(null, "chrome://browser/content/browser.xul", "_blank",
                                            features.join(","), args);

        WindowManager.updateGeometry(window, createData);

        // TODO: focused, type

        return new Promise(resolve => {
          window.addEventListener("load", function listener() {
            window.removeEventListener("load", listener);

            if (createData.state == "maximized" || createData.state == "normal" ||
                (createData.state == "fullscreen" && AppConstants.platform != "macosx")) {
              window.document.documentElement.setAttribute("sizemode", createData.state);
            } else if (createData.state !== null) {
              // window.minimize() has no useful effect until the window has
              // been shown.

              let obs = doc => {
                if (doc === window.document) {
                  Services.obs.removeObserver(obs, "document-shown");
                  WindowManager.setState(window, createData.state);
                  resolve();
                }
              };
              Services.obs.addObserver(obs, "document-shown", false);
              return;
            }

            resolve();
          });
        }).then(() => {
          return WindowManager.convert(extension, window);
        });
      },

      update: function(windowId, updateInfo) {
        if (updateInfo.state !== null && updateInfo.state != "normal") {
          if (updateInfo.left !== null || updateInfo.top !== null ||
              updateInfo.width !== null || updateInfo.height !== null) {
            return Promise.reject({message: `"state": "${updateInfo.state}" may not be combined with "left", "top", "width", or "height"`});
          }
        }

        let window = WindowManager.getWindow(windowId, context);
        if (updateInfo.focused) {
          Services.focus.activeWindow = window;
        }

        if (updateInfo.state !== null) {
          WindowManager.setState(window, updateInfo.state);
        }

        if (updateInfo.drawAttention) {
          // Bug 1257497 - Firefox can't cancel attention actions.
          window.getAttention();
        }

        WindowManager.updateGeometry(window, updateInfo);

        // TODO: All the other properties, focused=false...

        return Promise.resolve(WindowManager.convert(extension, window));
      },

      remove: function(windowId) {
        let window = WindowManager.getWindow(windowId, context);
        window.close();

        return new Promise(resolve => {
          let listener = () => {
            AllWindowEvents.removeListener("domwindowclosed", listener);
            resolve();
          };
          AllWindowEvents.addListener("domwindowclosed", listener);
        });
      },
    },
  };
});
