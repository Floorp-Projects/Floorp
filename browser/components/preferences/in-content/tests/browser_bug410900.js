/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/PlacesUtils.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");

function test() {
  waitForExplicitFinish();

  // Setup a phony handler to ensure the app pane will be populated.
  var handler = Cc["@mozilla.org/uriloader/web-handler-app;1"].
                createInstance(Ci.nsIWebHandlerApp);
  handler.name = "App pane alive test";
  handler.uriTemplate = "http://test.mozilla.org/%s";

  var extps = Cc["@mozilla.org/uriloader/external-protocol-service;1"].
              getService(Ci.nsIExternalProtocolService);
  var info = extps.getProtocolHandlerInfo("apppanetest");
  info.possibleApplicationHandlers.appendElement(handler, false);

  var hserv = Cc["@mozilla.org/uriloader/handler-service;1"].
              getService(Ci.nsIHandlerService);
  hserv.store(info);

  function observer(win, topic, data) {
    if (topic != "app-handler-pane-loaded")
      return;

    Services.obs.removeObserver(observer, "app-handler-pane-loaded");
    runTest(win);
  }
  Services.obs.addObserver(observer, "app-handler-pane-loaded", false);

  gBrowser.selectedTab = gBrowser.addTab("about:preferences");
}

function runTest(win) {
  win.gotoPref("applications");
  var sel = win.history.state;
  ok(sel == "applications", "Specified pane was opened");

  var rbox = win.document.getElementById("handlersView");
  ok(rbox, "handlersView is present");

  var items = rbox && rbox.getElementsByTagName("richlistitem");
  ok(items && items.length > 0, "App handler list populated");

  var handlerAdded = false;
  for (let i = 0; i < items.length; i++) {
    if (items[i].getAttribute('type') == "apppanetest")
      handlerAdded = true;
  }
  ok(handlerAdded, "apppanetest protocol handler was successfully added");

  gBrowser.removeCurrentTab();
  win.close();
  finish();
}
