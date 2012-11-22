/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8,<p>bug 660806 - history navigation must not show the autocomplete popup";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(HUD)
{
  content.wrappedJSObject.foobarBug660806 = {
    "location": "value0",
    "locationbar": "value1",
  };

  let jsterm = HUD.jsterm;
  let popup = jsterm.autocompletePopup;
  let onShown = function() {
    ok(false, "popup shown");
  };

  popup._panel.addEventListener("popupshown", onShown, false);

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
    popup._panel.removeEventListener("popupshown", onShown, false);
    executeSoon(finishTest);
  });
}
