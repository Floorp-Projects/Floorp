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
    Services.prefs.clearUserPref("browser.preferences.instantApply");
    Services.prefs.clearUserPref("network.proxy.share_proxy_settings");
    for (let proxyType of ["http", "ssl", "ftp", "socks"]) {
      Services.prefs.clearUserPref("network.proxy." + proxyType);
      Services.prefs.clearUserPref("network.proxy." + proxyType + "_port");
      if (proxyType == "http") {
        continue;
      }
      Services.prefs.clearUserPref("network.proxy.backup." + proxyType);
      Services.prefs.clearUserPref("network.proxy.backup." + proxyType + "_port");
    }
  });

  let connectionURL = "chrome://browser/content/preferences/connection.xul";
  let windowWatcher = Services.ww;

  // instantApply must be true, otherwise connection dialog won't save
  // when opened from in-content prefs
  Services.prefs.setBoolPref("browser.preferences.instantApply", true);

  // Set a shared proxy and a SOCKS backup
  Services.prefs.setIntPref("network.proxy.type", 1);
  Services.prefs.setBoolPref("network.proxy.share_proxy_settings", true);
  Services.prefs.setCharPref("network.proxy.http", "example.com");
  Services.prefs.setIntPref("network.proxy.http_port", 1200);
  Services.prefs.setCharPref("network.proxy.socks", "example.com");
  Services.prefs.setIntPref("network.proxy.socks_port", 1200);
  Services.prefs.setCharPref("network.proxy.backup.socks", "127.0.0.1");
  Services.prefs.setIntPref("network.proxy.backup.socks_port", 9050);

  // this observer is registered after the pref tab loads
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic == "domwindowopened") {
        // when connection window loads, run tests and acceptDialog()
        let win = aSubject.QueryInterface(Components.interfaces.nsIDOMWindow);
        win.addEventListener("load", function winLoadListener() {
          win.removeEventListener("load", winLoadListener, false);
          if (win.location.href == connectionURL) {
            ok(true, "connection window opened");
            win.document.documentElement.acceptDialog();
          }
        }, false);
      } else if (aTopic == "domwindowclosed") {
        // finish up when connection window closes
        let win = aSubject.QueryInterface(Components.interfaces.nsIDOMWindow);
        if (win.location.href == connectionURL) {
          windowWatcher.unregisterNotification(observer);
          ok(true, "connection window closed");

          // The SOCKS backup should not be replaced by the shared value
          is(Services.prefs.getCharPref("network.proxy.backup.socks"), "127.0.0.1", "Shared proxy backup shouldn't be replaced");
          is(Services.prefs.getIntPref("network.proxy.backup.socks_port"), 9050, "Shared proxy port backup shouldn't be replaced");

          gBrowser.removeCurrentTab();
          finish();
        }
      }
    }
  };

  /*
  The connection dialog alone won't save onaccept since it uses type="child",
  so it has to be opened as a sub dialog of the main pref tab.
  Open the main tab here.
  */
  open_preferences(function tabOpened(aContentWindow) {
    is(gBrowser.currentURI.spec, "about:preferences", "about:preferences loaded");
    windowWatcher.registerNotification(observer);
    gBrowser.contentWindow.gAdvancedPane.showConnections();
  });
}

