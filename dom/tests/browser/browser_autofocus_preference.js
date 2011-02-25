function test() {
  waitForExplicitFinish();

  var prefs = Components.classes["@mozilla.org/preferences-service;1"]
            .getService(Components.interfaces.nsIPrefBranch);
  var gAutofocusPref = prefs.getBoolPref("browser.autofocus");
  prefs.setBoolPref("browser.autofocus", false);

  gBrowser.selectedTab.linkedBrowser.addEventListener("load", function () {
    gBrowser.selectedTab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    executeSoon(function () {
      is(gBrowser.selectedTab.linkedBrowser.contentDocument.activeElement,
         gBrowser.selectedTab.linkedBrowser.contentDocument.body,
         "foo");

      prefs.setBoolPref("browser.autofocus", gAutofocusPref);

      finish();
    });
  }, true);

  gBrowser.selectedTab.linkedBrowser.loadURI("data:text/html,<!DOCTYPE html><html><body><input autofocus><button autofocus></button><textarea autofocus></textarea><select autofocus></select></body></html>");
}
