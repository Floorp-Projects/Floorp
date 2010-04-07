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

  openDialog("chrome://browser/content/preferences/preferences.xul", "Preferences",
             "chrome,titlebar,toolbar,centerscreen,dialog=no", "paneApplications");
}

function runTest(win) {
  var sel = win.document.documentElement.getAttribute("lastSelected");
  ok(sel == "paneApplications", "Specified pane was opened");

  var rbox = win.document.getElementById("handlersView");
  ok(rbox, "handlersView is present");

  var items = rbox && rbox.getElementsByTagName("richlistitem");
  ok(items && items.length > 0, "App handler list populated");

  var handlerAdded = false;
  for (let i = 0; i < items.length; i++) {
    if (items[i].type == "apppanetest")
      handlerAdded = true;
  }
  ok(handlerAdded, "apppanetest protocol handler was successfully added");

  win.close();
  finish();
}
