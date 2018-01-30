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

  let connectionURL = "chrome://browser/content/preferences/connection.xul";

  /*
  The connection dialog alone won't save onaccept since it uses type="child",
  so it has to be opened as a sub dialog of the main pref tab.
  Open the main tab here.
  */
  open_preferences(async function tabOpened(aContentWindow) {
    is(gBrowser.currentURI.spec, "about:preferences", "about:preferences loaded");
    let dialog = await openAndLoadSubDialog(connectionURL);
    let dialogClosingPromise = BrowserTestUtils.waitForEvent(dialog.document.documentElement, "dialogclosing");

    ok(dialog, "connection window opened");
    runConnectionTests(dialog);
    dialog.document.documentElement.acceptDialog();

    let dialogClosingEvent = await dialogClosingPromise;
    ok(dialogClosingEvent, "connection window closed");
    // runConnectionTests will have changed this pref - make sure it was
    // sanitized correctly when the dialog was accepted
    is(Services.prefs.getCharPref("network.proxy.no_proxies_on"),
       ".a.com,.b.com,.c.com", "no_proxies_on pref has correct value");
    gBrowser.removeCurrentTab();
    finish();
  });
}

// run a bunch of tests on the window containing connection.xul
function runConnectionTests(win) {
  let doc = win.document;
  let networkProxyNone = doc.getElementById("networkProxyNone");
  let networkProxyNonePref = win.Preferences.get("network.proxy.no_proxies_on");
  let networkProxyTypePref = win.Preferences.get("network.proxy.type");

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
