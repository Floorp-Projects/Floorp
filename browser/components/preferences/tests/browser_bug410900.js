function test() {
  waitForExplicitFinish();

  // setup a phony hander to ensure the app pane will be populated.
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

  openDialog("chrome://browser/content/preferences/preferences.xul", "Preferences",
             "chrome,titlebar,toolbar,centerscreen,dialog=no", "paneApplications");
  setTimeout(runTest, 250);
}

function runTest() {
  var wm = Cc["@mozilla.org/appshell/window-mediator;1"].
           getService(Ci.nsIWindowMediator);
  var win = wm.getMostRecentWindow("Browser:Preferences");
  ok(win, "Pref window opened");

  if (win) {
    var sel = win.document.documentElement.getAttribute("lastSelected");
    ok(sel == "paneApplications", "Specified pane was opened");

    var rbox = win.document.getElementById("handlersView");
    ok(rbox, "handlersView is present");

    var items = rbox && rbox.getElementsByTagName("richlistitem");
    todo(items && items.length > 0, "App handler list populated");

    win.close();
  }
  finish();
}
