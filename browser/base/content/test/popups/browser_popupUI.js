function test() {
  waitForExplicitFinish();
  SpecialPowers.pushPrefEnv({ set: [[ "dom.disable_open_during_load", false ]] });

  let popupOpened = BrowserTestUtils.waitForNewWindow({url: "about:blank"});
  BrowserTestUtils.openNewForegroundTab(gBrowser,
    "data:text/html,<html><script>popup=open('about:blank','','width=300,height=200')</script>"
  );
  popupOpened.then((win) => testPopupUI(win));
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
  is(gBrowser.browsers.length, 3, "Accel+T opened a new tab in the parent window");
  gBrowser.removeCurrentTab();

  EventUtils.synthesizeKey("w", { accelKey: true }, win);
  ok(win.closed, "Accel+W closes the popup");

  if (!win.closed)
    win.close();
  gBrowser.removeCurrentTab();
  finish();
}
