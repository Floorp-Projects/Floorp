/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();
  
  // network.proxy.type needs to be backed up and restored because mochitest
  // changes this setting from the default
  let oldNetworkProxyType = Services.prefs.getIntPref("network.proxy.type");
  registerCleanupFunction(function() {
    Services.prefs.setIntPref("network.proxy.type", oldNetworkProxyType);
    Services.prefs.clearUserPref("network.proxy.no_proxies_on");
    Services.prefs.clearUserPref("browser.preferences.instantApply");
  });
  
  let connectionURI = "chrome://browser/content/preferences/connection.xul";
  let windowWatcher = Cc["@mozilla.org/embedcomp/window-watcher;1"]
                          .getService(Components.interfaces.nsIWindowWatcher);

  // instantApply must be true, otherwise connection dialog won't save
  // when opened from in-content prefs
  Services.prefs.setBoolPref("browser.preferences.instantApply", true);

  // this observer is registered after the pref tab loads
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      
      if (aTopic == "domwindowopened") {
        // when connection window loads, run tests and acceptDialog()
        let win = aSubject.QueryInterface(Components.interfaces.nsIDOMWindow);
        win.addEventListener("load", function winLoadListener() {
          win.removeEventListener("load", winLoadListener, false);
          if (win.location.href == connectionURI) {
            ok(true, "connection window opened");
            runConnectionTests(win);
            win.document.documentElement.acceptDialog();
          }
        }, false);
      } else if (aTopic == "domwindowclosed") {
        // finish up when connection window closes
        let win = aSubject.QueryInterface(Components.interfaces.nsIDOMWindow);
        if (win.location.href == connectionURI) {
          windowWatcher.unregisterNotification(observer);
          ok(true, "connection window closed");
          // runConnectionTests will have changed this pref - make sure it was
          // sanitized correctly when the dialog was accepted
          is(Services.prefs.getCharPref("network.proxy.no_proxies_on"),
             ".a.com,.b.com,.c.com", "no_proxies_on pref has correct value");
          gBrowser.removeCurrentTab();
          finish();
        }
      }

    }
  }

  /*
  The connection dialog alone won't save onaccept since it uses type="child",
  so it has to be opened as a sub dialog of the main pref tab.
  Open the main tab here.
  */
  gBrowser.selectedTab = gBrowser.addTab("about:preferences");
  let newTabBrowser = gBrowser.getBrowserForTab(gBrowser.selectedTab);
  newTabBrowser.addEventListener("load", function tabLoadListener() {
    newTabBrowser.removeEventListener("load", tabLoadListener, true);
    is(gBrowser.currentURI.spec, "about:preferences", "about:preferences loaded");
    windowWatcher.registerNotification(observer);
    gBrowser.contentWindow.gAdvancedPane.showConnections();
  }, true);

}

// run a bunch of tests on the window containing connection.xul
function runConnectionTests(win) {
  let doc = win.document;
  let networkProxyNone = doc.getElementById("networkProxyNone");
  let networkProxyNonePref = doc.getElementById("network.proxy.no_proxies_on");
  let networkProxyTypePref = doc.getElementById("network.proxy.type");

  // make sure the networkProxyNone textbox is formatted properly
  is(networkProxyNone.getAttribute("multiline"), "true",
     "networkProxyNone textbox is multiline");
  is(networkProxyNone.getAttribute("rows"), "2",
     "networkProxyNone textbox has two rows");
  
  // check if sanitizing the given input for the no_proxies_on pref results in
  // expected string
  function testSanitize(input, expected, errorMessage) {
    networkProxyNonePref.value = input;
    win.gConnectionsDialog.sanitizeNoProxiesPref();
    is(networkProxyNonePref.value, expected, errorMessage);
  }

  // change this pref so proxy exceptions are actually configurable
  networkProxyTypePref.value = 1;
  is(networkProxyNone.disabled, false, "networkProxyNone textbox is enabled");

  testSanitize(".a.com", ".a.com",
               "sanitize doesn't mess up single filter");
  testSanitize(".a.com, .b.com, .c.com", ".a.com, .b.com, .c.com",
               "sanitize doesn't mess up multiple comma/space sep filters");
  testSanitize(".a.com\n.b.com", ".a.com,.b.com",
               "sanitize turns line break into comma");
  testSanitize(".a.com,\n.b.com", ".a.com,.b.com",
               "sanitize doesn't add duplicate comma after comma");
  testSanitize(".a.com\n,.b.com", ".a.com,.b.com",
               "sanitize doesn't add duplicate comma before comma");
  testSanitize(".a.com,\n,.b.com", ".a.com,,.b.com",
               "sanitize doesn't add duplicate comma surrounded by commas");
  testSanitize(".a.com, \n.b.com", ".a.com, .b.com",
               "sanitize doesn't add comma after comma/space");
  testSanitize(".a.com\n .b.com", ".a.com, .b.com",
               "sanitize adds comma before space");
  testSanitize(".a.com\n\n\n;;\n;\n.b.com", ".a.com,.b.com",
               "sanitize only adds one comma per substring of bad chars");
  testSanitize(".a.com,,.b.com", ".a.com,,.b.com",
               "duplicate commas from user are untouched");
  testSanitize(".a.com\n.b.com\n.c.com,\n.d.com,\n.e.com",
               ".a.com,.b.com,.c.com,.d.com,.e.com",
               "sanitize replaces things globally");

  // will check that this was sanitized properly after window closes
  networkProxyNonePref.value = ".a.com;.b.com\n.c.com";
}
