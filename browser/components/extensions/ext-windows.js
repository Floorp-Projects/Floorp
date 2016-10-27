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
  promiseObserved,
} = ExtensionUtils;

function onXULFrameLoaderCreated({target}) {
  target.messageManager.sendAsyncMessage("AllowScriptsToClose", {});
}

extensions.registerSchemaAPI("windows", "addon_parent", context => {
  let {extension} = context;
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
          // Wait a tick to avoid firing a superfluous WINDOW_ID_NONE
          // event when switching focus between two Firefox windows.
          Promise.resolve().then(() => {
            let window = Services.focus.activeWindow;
            let windowId = window ? WindowManager.getId(window) : WindowManager.WINDOW_ID_NONE;
            if (windowId !== lastOnFocusChangedWindowId) {
              fire(windowId);
              lastOnFocusChangedWindowId = windowId;
            }
          });
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

        let args = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);

        if (createData.tabId !== null) {
          if (createData.url !== null) {
            return Promise.reject({message: "`tabId` may not be used in conjunction with `url`"});
          }

          if (createData.allowScriptsToClose) {
            return Promise.reject({message: "`tabId` may not be used in conjunction with `allowScriptsToClose`"});
          }

          let tab = TabManager.getTab(createData.tabId, context);

          // Private browsing tabs can only be moved to private browsing
          // windows.
          let incognito = PrivateBrowsingUtils.isBrowserPrivate(tab.linkedBrowser);
          if (createData.incognito !== null && createData.incognito != incognito) {
            return Promise.reject({message: "`incognito` property must match the incognito state of tab"});
          }
          createData.incognito = incognito;

          args.appendElement(tab, /* weak = */ false);
        } else if (createData.url !== null) {
          if (Array.isArray(createData.url)) {
            let array = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
            for (let url of createData.url) {
              array.appendElement(mkstr(url), /* weak = */ false);
            }
            args.appendElement(array, /* weak = */ false);
          } else {
            args.appendElement(mkstr(createData.url), /* weak = */ false);
          }
        } else {
          args.appendElement(mkstr(aboutNewTabService.newTabURL), /* weak = */ false);
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

        let {allowScriptsToClose, url} = createData;
        if (allowScriptsToClose === null) {
          allowScriptsToClose = typeof url === "string" && url.startsWith("moz-extension://");
        }

        let window = Services.ww.openWindow(null, "chrome://browser/content/browser.xul", "_blank",
                                            features.join(","), args);

        WindowManager.updateGeometry(window, createData);

        // TODO: focused, type

        return new Promise(resolve => {
          window.addEventListener("load", function listener() {
            window.removeEventListener("load", listener);
            if (["maximized", "normal"].includes(createData.state)) {
              window.document.documentElement.setAttribute("sizemode", createData.state);
            }
            resolve(promiseObserved("browser-delayed-startup-finished", win => win == window));
          });
        }).then(() => {
          // Some states only work after delayed-startup-finished
          if (["minimized", "fullscreen", "docked"].includes(createData.state)) {
            WindowManager.setState(window, createData.state);
          }
          if (allowScriptsToClose) {
            for (let {linkedBrowser} of window.gBrowser.tabs) {
              onXULFrameLoaderCreated({target: linkedBrowser});
              linkedBrowser.addEventListener( // eslint-disable-line mozilla/balanced-listeners
                                             "XULFrameLoaderCreated", onXULFrameLoaderCreated);
            }
          }
          return WindowManager.convert(extension, window, {populate: true});
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
