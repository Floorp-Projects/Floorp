XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
  ignoreEvent,
  runSafe,
} = ExtensionUtils;

extensions.registerAPI((extension, context) => {
  return {
    windows: {
      WINDOW_ID_CURRENT: WindowManager.WINDOW_ID_CURRENT,
      WINDOW_ID_NONE: WindowManager.WINDOW_ID_NONE,

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

      get: function(windowId, getInfo, callback) {
        let window = WindowManager.getWindow(windowId);
        runSafe(context, callback, WindowManager.convert(extension, window, getInfo));
      },

      getCurrent: function(getInfo, callback) {
        if (!callback) {
          callback = getInfo;
          getInfo = {};
        }
        let window = currentWindow(context);
        runSafe(context, callback, WindowManager.convert(extension, window, getInfo));
      },

      getLastFocused: function(...args) {
        let getInfo, callback;
        if (args.length == 1) {
          callback = args[0];
        } else {
          [getInfo, callback] = args;
        }
        let window = WindowManager.topWindow;
        runSafe(context, callback, WindowManager.convert(extension, window, getInfo));
      },

      getAll: function(getInfo, callback) {
        let e = Services.wm.getEnumerator("navigator:browser");
        let windows = [];
        while (e.hasMoreElements()) {
          let window = e.getNext();
          if (window.document.readyState == "complete") {
            windows.push(WindowManager.convert(extension, window, getInfo));
          }
        }
        runSafe(context, callback, windows);
      },

      create: function(createData, callback) {
        function mkstr(s) {
          let result = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
          result.data = s;
          return result;
        }

        let args = Cc["@mozilla.org/supports-array;1"].createInstance(Ci.nsISupportsArray);
        if ("url" in createData) {
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
        if ("incognito" in createData) {
          if (createData.incognito) {
            extraFeatures += ",private";
          } else {
            extraFeatures += ",non-private";
          }
        }

        let window = Services.ww.openWindow(null, "chrome://browser/content/browser.xul", "_blank",
                                            "chrome,dialog=no,all" + extraFeatures, args);

        if ("left" in createData || "top" in createData) {
          let left = "left" in createData ? createData.left : window.screenX;
          let top = "top" in createData ? createData.top : window.screenY;
          window.moveTo(left, top);
        }
        if ("width" in createData || "height" in createData) {
          let width = "width" in createData ? createData.width : window.outerWidth;
          let height = "height" in createData ? createData.height : window.outerHeight;
          window.resizeTo(width, height);
        }

        // TODO: focused, type, state

        window.addEventListener("load", function listener() {
          window.removeEventListener("load", listener);
          if (callback) {
            runSafe(context, callback, WindowManager.convert(extension, window));
          }
        });
      },

      update: function(windowId, updateInfo, callback) {
        let window = WindowManager.getWindow(windowId);
        if (updateInfo.focused) {
          Services.focus.activeWindow = window;
        }
        // TODO: All the other properties...

        if (callback) {
          runSafe(context, callback, WindowManager.convert(extension, window));
        }
      },

      remove: function(windowId, callback) {
        let window = WindowManager.getWindow(windowId);
        window.close();

        let listener = () => {
          AllWindowEvents.removeListener("domwindowclosed", listener);
          if (callback) {
            runSafe(context, callback);
          }
        };
        AllWindowEvents.addListener("domwindowclosed", listener);
      },
    },
  };
});
