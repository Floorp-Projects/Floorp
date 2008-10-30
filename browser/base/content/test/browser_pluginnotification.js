const gTestRoot = "chrome://mochikit/content/browser/browser/base/content/test/";

var gTestBrowser = null;
var gNextTest = null;

function get_test_plugin() {
  var ph = Components.classes["@mozilla.org/plugin/host;1"]
                     .getService(Components.interfaces.nsIPluginHost);
  var tags = ph.getPluginTags({});
  
  // Find the test plugin
  for (var i = 0; i < tags.length; i++) {
    if (tags[i].name == "Test Plug-in")
      return tags[i];
  }
}

// This listens for the next opened window and checks it is of the right url.
// opencallback is called when the new window is fully loaded
// closecallback is called when the window is closed
function WindowOpenListener(url, opencallback, closecallback) {
  this.url = url;
  this.opencallback = opencallback;
  this.closecallback = closecallback;

  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator);
  wm.addListener(this);
}

WindowOpenListener.prototype = {
  url: null,
  opencallback: null,
  closecallback: null,
  window: null,
  domwindow: null,

  handleEvent: function(event) {
    is(this.domwindow.document.location.href, this.url, "Should have opened the correct window");

    this.domwindow.removeEventListener("load", this, false);
    // Allow any other load handlers to execute
    var self = this;
    executeSoon(function() { self.opencallback(self.domwindow); } );
  },

  onWindowTitleChange: function(window, title) {
  },

  onOpenWindow: function(window) {
    if (this.window)
      return;

    this.window = window;
    this.domwindow = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                           .getInterface(Components.interfaces.nsIDOMWindowInternal);
    this.domwindow.addEventListener("load", this, false);
  },

  onCloseWindow: function(window) {
    if (this.window != window)
      return;

    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Components.interfaces.nsIWindowMediator);
    wm.removeListener(this);
    this.opencallback = null;
    this.window = null;
    this.domwindow = null;

    // Let the window close complete
    executeSoon(this.closecallback);
    this.closecallback = null;
  }
};

function test() {
  waitForExplicitFinish();

  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);
  prepareTest(test1, gTestRoot + "plugin_unknown.html");
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
  executeSoon(gNextTest);
}

function prepareTest(nextTest, url) {
  gNextTest = nextTest;
  gTestBrowser.contentWindow.location = url;
}

// Tests a page with an unknown plugin in it.
function test1() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(notificationBox.getNotificationWithValue("missing-plugins"), "Test 1, Should have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 1, Should not have displayed the blocked plugin notification");
  ok(gTestBrowser.missingPlugins, "Test 1, Should be a missing plugin list");
  ok("application/x-unknown" in gTestBrowser.missingPlugins, "Test 1, Should know about application/x-unknown");
  ok(!("application/x-test" in gTestBrowser.missingPlugins), "Test 1, Should not know about application/x-test");

  var plugin = get_test_plugin();
  ok(plugin, "Should have a test plugin");
  plugin.disabled = false;
  plugin.blocklisted = false;
  prepareTest(test2, gTestRoot + "plugin_test.html");
}

// Tests a page with a working plugin in it.
function test2() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 2, Should not have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 2, Should not have displayed the blocked plugin notification");
  ok(!gTestBrowser.missingPlugins, "Test 2, Should not be a missing plugin list");

  var plugin = get_test_plugin();
  ok(plugin, "Should have a test plugin");
  plugin.disabled = true;
  prepareTest(test3, gTestRoot + "plugin_test.html");
}

// Tests a page with a disabled plugin in it.
function test3() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 3, Should not have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 3, Should not have displayed the blocked plugin notification");
  ok(!gTestBrowser.missingPlugins, "Test 3, Should not be a missing plugin list");

  new WindowOpenListener("chrome://mozapps/content/extensions/extensions.xul", test4, prepareTest5);

  EventUtils.synthesizeMouse(gTestBrowser.contentDocument.getElementById("test"),
                             0, 0, {}, gTestBrowser.contentWindow);
}

function test4(win) {
  is(win.gView, "plugins", "Should have displayed the plugins pane");
  win.close();
}

function prepareTest5() {
  var plugin = get_test_plugin();
  plugin.disabled = false;
  plugin.blocklisted = true;
  prepareTest(test5, gTestRoot + "plugin_test.html");
}

// Tests a page with a blocked plugin in it.
function test5() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 5, Should not have displayed the missing plugin notification");
  ok(notificationBox.getNotificationWithValue("blocked-plugins"), "Test 5, Should have displayed the blocked plugin notification");
  ok(gTestBrowser.missingPlugins, "Test 5, Should be a missing plugin list");
  ok("application/x-test" in gTestBrowser.missingPlugins, "Test 5, Should know about application/x-test");
  ok(!("application/x-unknown" in gTestBrowser.missingPlugins), "Test 5, Should not know about application/x-unknown");

  prepareTest(test6, gTestRoot + "plugin_both.html");
}

// Tests a page with a blocked and unknown plugin in it.
function test6() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(notificationBox.getNotificationWithValue("missing-plugins"), "Test 6, Should have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 6, Should not have displayed the blocked plugin notification");
  ok(gTestBrowser.missingPlugins, "Test 6, Should be a missing plugin list");
  ok("application/x-unknown" in gTestBrowser.missingPlugins, "Test 6, Should know about application/x-unknown");
  ok("application/x-test" in gTestBrowser.missingPlugins, "Test 6, Should know about application/x-test");

  prepareTest(test7, gTestRoot + "plugin_both2.html");
}

// Tests a page with a blocked and unknown plugin in it (alternate order to above).
function test7() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(notificationBox.getNotificationWithValue("missing-plugins"), "Test 7, Should have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 7, Should not have displayed the blocked plugin notification");
  ok(gTestBrowser.missingPlugins, "Test 7, Should be a missing plugin list");
  ok("application/x-unknown" in gTestBrowser.missingPlugins, "Test 7, Should know about application/x-unknown");
  ok("application/x-test" in gTestBrowser.missingPlugins, "Test 7, Should know about application/x-test");

  var plugin = get_test_plugin();
  plugin.disabled = false;
  plugin.blocklisted = false;
  finishTest();
}
