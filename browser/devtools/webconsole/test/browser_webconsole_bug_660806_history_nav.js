/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html,<p>bug 660806 - history navigation must not show the autocomplete popup";

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.gcli.enable");
});

function test() {
  Services.prefs.setBoolPref("devtools.gcli.enable", false);
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoaded, true);
}

function tabLoaded()
{
  browser.removeEventListener("load", tabLoaded, true);
  openConsole();

  content.wrappedJSObject.foobarBug660806 = {
    "location": "value0",
    "locationbar": "value1",
  };

  let hudId = HUDService.getHudIdByWindow(content);
  let HUD = HUDService.hudReferences[hudId];
  let jsterm = HUD.jsterm;
  let popup = jsterm.autocompletePopup;
  popup._panel.addEventListener("popupshown", function() {
    popup._panel.removeEventListener("popupshown", arguments.callee, false);
    ok(false, "popup shown");
  }, false);

  ok(!popup.isOpen, "popup is not open");

  ok(!jsterm.lastInputValue, "no lastInputValue");
  jsterm.setInputValue("window.foobarBug660806.location");
  is(jsterm.lastInputValue, "window.foobarBug660806.location",
     "lastInputValue is correct");

  EventUtils.synthesizeKey("VK_RETURN", {});
  EventUtils.synthesizeKey("VK_UP", {});

  is(jsterm.lastInputValue, "window.foobarBug660806.location",
     "lastInputValue is correct, again");

  executeSoon(function() {
    ok(!popup.isOpen, "popup is not open");
    executeSoon(finishTest);
  });
}
