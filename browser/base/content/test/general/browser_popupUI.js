function test() {
  waitForExplicitFinish();
  gPrefService.setBoolPref("dom.disable_open_during_load", false);

  gBrowser.selectedTab = gBrowser.addTab();

  var browser = gBrowser.selectedBrowser;
  browser.addEventListener("load", function () {
    browser.removeEventListener("load", arguments.callee, true);

    if (gPrefService.prefHasUserValue("dom.disable_open_during_load"))
      gPrefService.clearUserPref("dom.disable_open_during_load");

    findPopup();
  }, true);

  content.location =
    "data:text/html,<html><script>popup=open('about:blank','','width=300,height=200')</script>";
}

function findPopup() {
  var enumerator = Services.wm.getEnumerator("navigator:browser");

  while (enumerator.hasMoreElements()) {
    let win = enumerator.getNext();
    if (win.content.wrappedJSObject == content.wrappedJSObject.popup) {
      testPopupUI(win);
      return;
    }
  }

  throw "couldn't find the popup";
}

function testPopupUI(win) {
  var doc = win.document;

  ok(win.gURLBar, "location bar exists in the popup");
  isnot(win.gURLBar.clientWidth, 0, "location bar is visible in the popup");
  ok(win.gURLBar.readOnly, "location bar is read-only in the popup");
  isnot(doc.getElementById("Browser:OpenLocation").getAttribute("disabled"), "true",
     "'open location' command is not disabled in the popup");

  let historyButton = doc.getAnonymousElementByAttribute(win.gURLBar, "anonid",
                                                         "historydropmarker");
  is(historyButton.clientWidth, 0, "history dropdown button is hidden in the popup");

  EventUtils.synthesizeKey("t", { accelKey: true }, win);
  is(win.gBrowser.browsers.length, 1, "Accel+T doesn't open a new tab in the popup");

  EventUtils.synthesizeKey("w", { accelKey: true }, win);
  ok(win.closed, "Accel+W closes the popup");

  if (!win.closed)
    win.close();
  gBrowser.removeCurrentTab();
  finish();
}
