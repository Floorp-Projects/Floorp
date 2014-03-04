/* This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var rootDir = getRootDirectory(gTestPath);
const gHttpTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
const Cc = Components.classes;
const Ci = Components.interfaces;
var gTestBrowser = null;
var gConsoleErrors = 0;

function test() {
  waitForExplicitFinish();
  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("PluginBindingAttached", pluginBindingAttached, true, true);
  var consoleService = Cc["@mozilla.org/consoleservice;1"]
                         .getService(Ci.nsIConsoleService);
  var errorListener = {
    observe: function(aMessage) {
      if (aMessage.message.contains("NS_ERROR"))
        gConsoleErrors++;
    }
  };
  consoleService.registerListener(errorListener);
  registerCleanupFunction(function() {
    gTestBrowser.removeEventListener("PluginBindingAttached", pluginBindingAttached, true);
    consoleService.unregisterListener(errorListener);
    gBrowser.removeCurrentTab();
    window.focus();
  });
  gTestBrowser.contentWindow.location = gHttpTestRoot + "plugin_bug797677.html";
}

function pluginBindingAttached() {
  // Let browser-plugins.js handle the PluginNotFound event, then run the test
  executeSoon(runTest);
}

function runTest() {
  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("plugin");
  ok(plugin, "plugin should be in the page");
  is(gConsoleErrors, 0, "should have no console errors");
  finish();
}
