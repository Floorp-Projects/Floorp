/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir;

var gTestBrowser = null;
var gNextTest = null;
var gNextTestSkip = 0;

var gPlayPreviewRegistration = null;

var gTestPluginType = 'application/x-test';
var gTestPluginPreviewUrl = 'about:';

function registerPlayPreview(whitelist) {
  var ph = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  ph.registerPlayPreviewMimeType(gTestPluginType, true,
                                 gTestPluginPreviewUrl, whitelist);

  return (gPlayPreviewRegistration = {
    unregister: function() {
      ph.unregisterPlayPreviewMimeType(gTestPluginType);
      gPlayPreviewRegistration = null;
    }
  });
}

function unregisterPlayPreview() {
  gPlayPreviewRegistration.unregister();
}

Components.utils.import('resource://gre/modules/XPCOMUtils.jsm');
Components.utils.import("resource://gre/modules/Services.jsm");


function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
    if (gPlayPreviewRegistration) {
      gPlayPreviewRegistration.unregister();
    }
  });

  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED);

  registerPlayPreview('@*plugin_test.html');
  prepareTest(test1a, gTestRoot + "plugin_test.html", 1);
}

function finishTest() {
  gTestBrowser.removeEventListener("load", pageLoad, true);
  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}

function pageLoad() {
  // The plugin events are async dispatched and can come after the load event
  // This just allows the events to fire before we then go on to test the states

  // iframe might triggers load event as well, making sure we skip some to let
  // all iframes on the page be loaded as well
  if (gNextTestSkip) {
    gNextTestSkip--;
    return;
  }
  executeSoon(gNextTest);
}

function prepareTest(nextTest, url, skip) {
  gNextTest = nextTest;
  gNextTestSkip = skip;
  gTestBrowser.contentWindow.location = url;
}

// Tests plugin on a whitelisted page.
function test1a() {
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_PLAY_PREVIEW, "Test 1a, plugin fallback type should be PLUGIN_PLAY_PREVIEW");
  ok(!objLoadingContent.activated, "Test 1a, Plugin should not be activated");

  unregisterPlayPreview();

  registerPlayPreview('@*plugin_wrong.html');
  var plugin = getTestPlugin();
  plugin.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
  prepareTest(test1b, gTestRoot + "plugin_test.html");
}

// Tests plugin on a non-whitelisted page.
function test1b() {
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 1b, Plugin should be activated");

  unregisterPlayPreview();

  registerPlayPreview('*browser_pluginplaypreview3.js');
  var plugin = getTestPlugin();
  plugin.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
  prepareTest(test2a, gTestRoot + "plugin_test_w_src.html");
}

// Tests plugin with whitelisted src/data.
function test2a() {
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 2a, Plugin should not be activated");

  unregisterPlayPreview();

  registerPlayPreview('*plugin_test_w_src.html');
  var plugin = getTestPlugin();
  plugin.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
  prepareTest(test2b, gTestRoot + "plugin_test_w_src.html");
}

// Tests plugin with non-whitelisted src/data.
function test2b() {
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 2b, Plugin should be activated");

  finishTest();
}

