/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
} = ExtensionUtils;

extensions.registerSchemaAPI("windows", null, (extension, context) => {
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
        // FIXME: This will send multiple messages for a single focus change.
        let listener = event => {
          let window = WindowManager.topWindow;
          let windowId = window ? WindowManager.getId(window) : WindowManager.WINDOW_ID_NONE;
          fire(windowId);
        };
        AllWindowEvents.addListener("focus", listener);
        AllWindowEvents.addListener("blur", listener);
        return () => {
          AllWindowEvents.removeListener("focus", listener);
          AllWindowEvents.removeListener("blur", listener);
        };
      }).api(),

      get: function(windowId, getInfo) {
        let window = WindowManager.getWindow(windowId);
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
        function mkstr(s) {
          let result = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
          result.data = s;
          return result;
        }

        let args = Cc["@mozilla.org/supports-array;1"].createInstance(Ci.nsISupportsArray);
        if (createData.url !== null) {
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

        let extraFeatures = "";
        if (createData.incognito !== null) {
          if (createData.incognito) {
            extraFeatures += ",private";
          } else {
            extraFeatures += ",non-private";
          }
        }

        let window = Services.ww.openWindow(null, "chrome://browser/content/browser.xul", "_blank",
                                            "chrome,dialog=no,all" + extraFeatures, args);

        if (createData.left !== null || createData.top !== null) {
          let left = createData.left !== null ? createData.left : window.screenX;
          let top = createData.top !== null ? createData.top : window.screenY;
          window.moveTo(left, top);
        }
        if (createData.width !== null || createData.height !== null) {
          let width = createData.width !== null ? createData.width : window.outerWidth;
          let height = createData.height !== null ? createData.height : window.outerHeight;
          window.resizeTo(width, height);
        }

        // TODO: focused, type, state

        return new Promise(resolve => {
          window.addEventListener("load", function listener() {
            window.removeEventListener("load", listener);
            resolve(WindowManager.convert(extension, window));
          });
        });
      },

      update: function(windowId, updateInfo) {
        let window = WindowManager.getWindow(windowId);
        if (updateInfo.focused) {
          Services.focus.activeWindow = window;
        }
        // TODO: All the other properties...

        return Promise.resolve(WindowManager.convert(extension, window));
      },

      remove: function(windowId) {
        let window = WindowManager.getWindow(windowId);
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
