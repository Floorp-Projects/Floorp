/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  // Open a new tab, since starting a drag from the home button activates it and
  // we don't want to interfere with future tests by loading the home page.
  let newTab = gBrowser.selectedTab = gBrowser.addTab();
  registerCleanupFunction(function () {
    gBrowser.removeTab(newTab);
  });

  let scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                     getService(Ci.mozIJSSubScriptLoader);
  let chromeUtils = {};
  scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/ChromeUtils.js", chromeUtils);

  let homeButton = document.getElementById("home-button");
  ok(homeButton, "home button present");

  let dialogListener = new WindowListener("chrome://global/content/commonDialog.xul", function (domwindow) {
    ok(true, "dialog appeared in response to home button drop");
    domwindow.document.documentElement.cancelDialog();
    Services.wm.removeListener(dialogListener);

    // Now trigger the invalid URI test
    executeSoon(function () {
      let consoleListener = {
        observe: function (m) {
          if (m.message.indexOf("NS_ERROR_DOM_BAD_URI") > -1) {
            ok(true, "drop was blocked");
            executeSoon(finish);
          }
        }
      }
      Services.console.registerListener(consoleListener);
      registerCleanupFunction(function () {
        Services.console.unregisterListener(consoleListener);
      });

      executeSoon(function () {
        info("Attempting second drop, of a javascript: URI");
        // The drop handler throws an exception when dragging URIs that inherit
        // principal, e.g. javascript:
        expectUncaughtException();
        chromeUtils.synthesizeDrop(homeButton, homeButton, [[{type: "text/plain", data: "javascript:8888"}]], "copy", window, EventUtils);
      });
    })
  });

  Services.wm.addListener(dialogListener);

  chromeUtils.synthesizeDrop(homeButton, homeButton, [[{type: "text/plain", data: "http://mochi.test:8888/"}]], "copy", window, EventUtils);
}

function WindowListener(aURL, aCallback) {
  this.callback = aCallback;
  this.url = aURL;
}
WindowListener.prototype = {
  onOpenWindow: function(aXULWindow) {
    var domwindow = aXULWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIDOMWindow);
    var self = this;
    domwindow.addEventListener("load", function() {
      domwindow.removeEventListener("load", arguments.callee, false);

      ok(true, "domwindow.document.location.href: " + domwindow.document.location.href);
      if (domwindow.document.location.href != self.url)
        return;

      // Allow other window load listeners to execute before passing to callback
      executeSoon(function() {
        self.callback(domwindow);
      });
    }, false);
  },
  onCloseWindow: function(aXULWindow) {},
  onWindowTitleChange: function(aXULWindow, aNewTitle) {}
}

