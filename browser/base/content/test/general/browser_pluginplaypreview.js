/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir;

var gTestBrowser = null;
var gNextTest = null;
var gNextTestSkip = 0;
var gPlayPreviewPluginActualEvents = 0;
var gPlayPreviewPluginExpectedEvents = 1;

var gPlayPreviewRegistration = null;

function registerPlayPreview(mimeType, targetUrl) {

  function StreamConverterFactory() {}
  StreamConverterFactory.prototype = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory]),
    _targetConstructor: null,

    register: function register(targetConstructor) {
      this._targetConstructor = targetConstructor;
      var proto = targetConstructor.prototype;
      var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
      registrar.registerFactory(proto.classID, proto.classDescription,
                                proto.contractID, this);
    },

    unregister: function unregister() {
      var proto = this._targetConstructor.prototype;
      var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
      registrar.unregisterFactory(proto.classID, this);
      this._targetConstructor = null;
    },

    // nsIFactory
    createInstance: function createInstance(aOuter, iid) {
      if (aOuter !== null)
        throw Cr.NS_ERROR_NO_AGGREGATION;
      return (new (this._targetConstructor)).QueryInterface(iid);
    },

    // nsIFactory
    lockFactory: function lockFactory(lock) {
      // No longer used as of gecko 1.7.
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    }
  };

  function OverlayStreamConverter() {}
  OverlayStreamConverter.prototype = {
    QueryInterface: XPCOMUtils.generateQI([
        Ci.nsISupports,
        Ci.nsIStreamConverter,
        Ci.nsIStreamListener,
        Ci.nsIRequestObserver
    ]),

    classID: Components.ID('{4c6030f7-e20a-264f-0f9b-ada3a9e97384}'),
    classDescription: 'overlay-test-data Component',
    contractID: '@mozilla.org/streamconv;1?from=application/x-moz-playpreview&to=*/*',

    // nsIStreamConverter::convert
    convert: function(aFromStream, aFromType, aToType, aCtxt) {
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    },

    // nsIStreamConverter::asyncConvertData
    asyncConvertData: function(aFromType, aToType, aListener, aCtxt) {
      var isValidRequest = false;
      try {
        var request = aCtxt;
        request.QueryInterface(Ci.nsIChannel);
        var spec = request.URI.spec;
        var expectedSpec = 'data:application/x-moz-playpreview;,' + mimeType;
        isValidRequest = (spec == expectedSpec);
      } catch (e) { }
      if (!isValidRequest)
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;

      // Store the listener passed to us
      this.listener = aListener;
    },

    // nsIStreamListener::onDataAvailable
    onDataAvailable: function(aRequest, aContext, aInputStream, aOffset, aCount) {
      // Do nothing since all the data loading is handled by the viewer.
      ok(false, "onDataAvailable should not be called");
    },

    // nsIRequestObserver::onStartRequest
    onStartRequest: function(aRequest, aContext) {

      // Setup the request so we can use it below.
      aRequest.QueryInterface(Ci.nsIChannel);
      // Cancel the request so the viewer can handle it.
      aRequest.cancel(Cr.NS_BINDING_ABORTED);

      // Create a new channel that is viewer loaded as a resource.
      var ioService = Services.io;
      var channel = ioService.newChannel(targetUrl, null, null);
      channel.asyncOpen(this.listener, aContext);
    },

    // nsIRequestObserver::onStopRequest
    onStopRequest: function(aRequest, aContext, aStatusCode) {
      // Do nothing.
    }
  };

  var ph = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  ph.registerPlayPreviewMimeType(mimeType, true); // ignoring CTP rules

  var factory = new StreamConverterFactory();
  factory.register(OverlayStreamConverter);

  return (gPlayPreviewRegistration = {
    unregister: function() {
      ph.unregisterPlayPreviewMimeType(mimeType);
      factory.unregister();
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
    if (gPlayPreviewRegistration)
      gPlayPreviewRegistration.unregister();
    Services.prefs.clearUserPref("plugins.click_to_play");
  });

  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);
  gTestBrowser.addEventListener("PluginBindingAttached", handleBindingAttached, true, true);

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED);
  registerPlayPreview('application/x-test', 'about:');
  prepareTest(test1a, gTestRoot + "plugin_test.html", 1);
}

function finishTest() {
  gTestBrowser.removeEventListener("load", pageLoad, true);
  gTestBrowser.removeEventListener("PluginBindingAttached", handleBindingAttached, true, true);
  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}

function handleBindingAttached(evt) {
  if (evt.target instanceof Ci.nsIObjectLoadingContent &&
      evt.target.pluginFallbackType == Ci.nsIObjectLoadingContent.PLUGIN_PLAY_PREVIEW)
    gPlayPreviewPluginActualEvents++;
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

// Tests a page with normal play preview registration (1/2)
function test1a() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 1a, Should not have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 1a, Should not have displayed the blocked plugin notification");

  var pluginInfo = getTestPlugin();
  ok(pluginInfo, "Should have a test plugin");

  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_PLAY_PREVIEW, "Test 1a, plugin fallback type should be PLUGIN_PLAY_PREVIEW");
  ok(!objLoadingContent.activated, "Test 1a, Plugin should not be activated");

  var overlay = doc.getAnonymousElementByAttribute(plugin, "class", "previewPluginContent");
  ok(overlay, "Test 1a, the overlay div is expected");

  var iframe = overlay.getElementsByClassName("previewPluginContentFrame")[0];
  ok(iframe && iframe.localName == "iframe", "Test 1a, the overlay iframe is expected");
  var iframeHref = iframe.contentWindow.location.href;
  ok(iframeHref == "about:", "Test 1a, the overlay about: content is expected");

  var rect = iframe.getBoundingClientRect();
  ok(rect.width == 200, "Test 1a, Plugin with id=" + plugin.id + " overlay rect should have 200px width before being replaced by actual plugin");
  ok(rect.height == 200, "Test 1a, Plugin with id=" + plugin.id + " overlay rect should have 200px height before being replaced by actual plugin");

  var e = overlay.ownerDocument.createEvent("CustomEvent");
  e.initCustomEvent("MozPlayPlugin", true, true, null);
  overlay.dispatchEvent(e);
  var condition = function() objLoadingContent.activated;
  waitForCondition(condition, test1b, "Test 1a, Waited too long for plugin to stop play preview");
}

// Tests that activating via MozPlayPlugin through the notification works (part 2/2)
function test1b() {
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 1b, Plugin should be activated");

  is(gPlayPreviewPluginActualEvents, gPlayPreviewPluginExpectedEvents,
     "There should be exactly one PluginPlayPreview event");

  unregisterPlayPreview();

  prepareTest(test2, gTestRoot + "plugin_test.html");
}

// Tests a page with a working plugin in it -- the mime type was just unregistered.
function test2() {
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 2, Plugin should be activated");

  registerPlayPreview('application/x-unknown', 'about:');

  prepareTest(test3, gTestRoot + "plugin_test.html");
}

// Tests a page with a working plugin in it -- diffent play preview type is reserved.
function test3() {
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 3, Plugin should be activated");

  unregisterPlayPreview();

  registerPlayPreview('application/x-test', 'about:');
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY);
  prepareTest(test4a, gTestRoot + "plugin_test.html", 1);
}

// Test a fallback to the click-to-play
function test4a() {
  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_PLAY_PREVIEW, "Test 4a, plugin fallback type should be PLUGIN_PLAY_PREVIEW");
  ok(!objLoadingContent.activated, "Test 4a, Plugin should not be activated");

  var overlay = doc.getAnonymousElementByAttribute(plugin, "class", "previewPluginContent");
  ok(overlay, "Test 4a, the overlay div is expected");

  var e = overlay.ownerDocument.createEvent("CustomEvent");
  e.initCustomEvent("MozPlayPlugin", true, true, true);
  overlay.dispatchEvent(e);
  var condition = function() objLoadingContent.pluginFallbackType == Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY;
  waitForCondition(condition, test4b, "Test 4a, Waited too long for plugin to stop play preview");
}

function test4b() {
  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.pluginFallbackType != Ci.nsIObjectLoadingContent.PLUGIN_PLAY_PREVIEW, "Test 4b, plugin fallback type should not be PLUGIN_PLAY_PREVIEW");
  ok(!objLoadingContent.activated, "Test 4b, Plugin should not be activated");

  prepareTest(test5a, gTestRoot + "plugin_test.html", 1);
}

// Test a bypass of the click-to-play
function test5a() {
  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_PLAY_PREVIEW, "Test 5a, plugin fallback type should be PLUGIN_PLAY_PREVIEW");
  ok(!objLoadingContent.activated, "Test 5a, Plugin should not be activated");

  var overlay = doc.getAnonymousElementByAttribute(plugin, "class", "previewPluginContent");
  ok(overlay, "Test 5a, the overlay div is expected");

  var e = overlay.ownerDocument.createEvent("CustomEvent");
  e.initCustomEvent("MozPlayPlugin", true, true, false);
  overlay.dispatchEvent(e);
  var condition = function() objLoadingContent.activated;
  waitForCondition(condition, test5b, "Test 5a, Waited too long for plugin to stop play preview");
}

function test5b() {
  var doc = gTestBrowser.contentDocument;
  var plugin = doc.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 5b, Plugin should be activated");

  finishTest();
}

