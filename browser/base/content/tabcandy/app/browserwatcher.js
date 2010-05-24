// Title: browserwatcher.js
(function(){

Utils.log('warning! browserwatcher.js has not been tested since the code was taken from tabs.js!');

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

// ##########
// Class: WindowWatcher
function WindowWatcher() {
  var self = this;

  var observer = {
    observe: function(window, event) {
      if (event == "domwindowopened") {
        if (self.onWindowOpened)
          self.onWindowOpened(window);
      } else
        if (self.onWindowClosed)
          self.onWindowClosed(window);
    }
  };

  var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"]
           .getService(Ci.nsIWindowWatcher);
  ww.registerNotification(observer);

  Extension.addUnloadMethod(
    this,
    function() {
      ww.unregisterNotification(observer);
    });
}

// ##########
// Class: BrowserWatcher
// When this object is instantiated, the given onLoad() is called for
// all browser windows, and subsequently for all newly-opened browser
// windows. When a browser window closes, onUnload() is called.
// onUnload() is also called once for each browser window when the
// extension is unloaded.
function BrowserWatcher(options) {
  var pendingHandlers = [];

  function makeSafeFunc(func) {
    function safeFunc(window) {
      try {
        func(window);
      } catch (e) {
        Utils.log(e);
      }
    };
    return safeFunc;
  }

  function addUnloader(chromeWindow, func) {
    function onUnload() {
      chromeWindow.removeEventListener("unload", onUnload, false);
      pendingHandlers.splice(pendingHandlers.indexOf(onUnload), 1);
      func(chromeWindow);
    }
    pendingHandlers.push(onUnload);
    chromeWindow.addEventListener("unload", onUnload, false);
  }

  function loadAndBind(chromeWindow) {
    if (options.onLoad)
      (makeSafeFunc(options.onLoad))(chromeWindow);
    if (options.onUnload)
      addUnloader(chromeWindow, makeSafeFunc(options.onUnload));
  }

  var wm = Cc["@mozilla.org/appshell/window-mediator;1"]
           .getService(Ci.nsIWindowMediator);

  var enumerator = wm.getEnumerator(XULApp.appWindowType);

  while (enumerator.hasMoreElements()) {
    var chromeWindow = enumerator.getNext();
    if (chromeWindow.gIsDoneLoading)
      loadAndBind(chromeWindow);
    else
      onWindowOpened(chromeWindow);
  }

  function onWindowOpened(chromeWindow) {
    function removeListener() {
      chromeWindow.removeEventListener("load", onLoad, false);
      pendingHandlers.splice(pendingHandlers.indexOf(removeListener), 1);
    }
    function onLoad() {
      removeListener();
      var type = chromeWindow.document.documentElement
                 .getAttribute("windowtype");
      if (type == XULApp.appWindowType)
        loadAndBind(chromeWindow);
    }
    chromeWindow.addEventListener("load", onLoad, false);
    pendingHandlers.push(removeListener);
  }

  var ww = new WindowWatcher();
  ww.onWindowOpened = onWindowOpened;

  Extension.addUnloadMethod(
    this,
    function() {
      ww.unload();
      var handlers = pendingHandlers.slice();
      handlers.forEach(function(handler) { handler(); });
    });
}

})();