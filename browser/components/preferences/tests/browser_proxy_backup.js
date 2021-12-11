/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();

  // network.proxy.type needs to be backed up and restored because mochitest
  // changes this setting from the default
  let oldNetworkProxyType = Services.prefs.getIntPref("network.proxy.type");
  registerCleanupFunction(function() {
    Services.prefs.setIntPref("network.proxy.type", oldNetworkProxyType);
    Services.prefs.clearUserPref("browser.preferences.instantApply");
    Services.prefs.clearUserPref("network.proxy.share_proxy_settings");
    for (let proxyType of ["http", "ssl", "socks"]) {
      Services.prefs.clearUserPref("network.proxy." + proxyType);
      Services.prefs.clearUserPref("network.proxy." + proxyType + "_port");
      if (proxyType == "http") {
        continue;
      }
      Services.prefs.clearUserPref("network.proxy.backup." + proxyType);
      Services.prefs.clearUserPref(
        "network.proxy.backup." + proxyType + "_port"
      );
    }
    // On accepting the dialog, we also write TRR values, so we need to clear
    // them. They are tested separately in browser_connect_dnsoverhttps.js.
    Services.prefs.clearUserPref("network.trr.mode");
    Services.prefs.clearUserPref("network.trr.uri");
  });

  let connectionURL =
    "chrome://browser/content/preferences/dialogs/connection.xhtml";

  // Set a shared proxy and an SSL backup
  Services.prefs.setIntPref("network.proxy.type", 1);
  Services.prefs.setBoolPref("network.proxy.share_proxy_settings", true);
  Services.prefs.setCharPref("network.proxy.http", "example.com");
  Services.prefs.setIntPref("network.proxy.http_port", 1200);
  Services.prefs.setCharPref("network.proxy.ssl", "example.com");
  Services.prefs.setIntPref("network.proxy.ssl_port", 1200);
  Services.prefs.setCharPref("network.proxy.backup.ssl", "127.0.0.1");
  Services.prefs.setIntPref("network.proxy.backup.ssl_port", 9050);

  /*
  The connection dialog alone won't save onaccept since it uses type="child",
  so it has to be opened as a sub dialog of the main pref tab.
  Open the main tab here.
  */
  open_preferences(async function tabOpened(aContentWindow) {
    is(
      gBrowser.currentURI.spec,
      "about:preferences",
      "about:preferences loaded"
    );
    let dialog = await openAndLoadSubDialog(connectionURL);
    let dialogElement = dialog.document.getElementById("ConnectionsDialog");
    let dialogClosingPromise = BrowserTestUtils.waitForEvent(
      dialogElement,
      "dialogclosing"
    );

    ok(dialog, "connection window opened");
    dialogElement.acceptDialog();

    let dialogClosingEvent = await dialogClosingPromise;
    ok(dialogClosingEvent, "connection window closed");

    // The SSL backup should not be replaced by the shared value
    is(
      Services.prefs.getCharPref("network.proxy.backup.ssl"),
      "127.0.0.1",
      "Shared proxy backup shouldn't be replaced"
    );
    is(
      Services.prefs.getIntPref("network.proxy.backup.ssl_port"),
      9050,
      "Shared proxy port backup shouldn't be replaced"
    );

    gBrowser.removeCurrentTab();
    finish();
  });
}
