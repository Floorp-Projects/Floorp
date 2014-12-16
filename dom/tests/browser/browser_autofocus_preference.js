function test() {
  waitForExplicitFinish();

  var prefs = Components.classes["@mozilla.org/preferences-service;1"]
            .getService(Components.interfaces.nsIPrefBranch);
  var gAutofocusPref = prefs.getBoolPref("browser.autofocus");
  prefs.setBoolPref("browser.autofocus", false);

  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    executeSoon(function () {
      is(gBrowser.selectedBrowser.contentDocument.activeElement,
         gBrowser.selectedBrowser.contentDocument.body,
         "foo");

      prefs.setBoolPref("browser.autofocus", gAutofocusPref);

      finish();
    });
  }, true);

  gBrowser.selectedBrowser.loadURI("data:text/html,<!DOCTYPE html><html><body><input autofocus><button autofocus></button><textarea autofocus></textarea><select autofocus></select></body></html>");
}
