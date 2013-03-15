/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "data:text/html;charset=utf-8,<p>bug 585991 - autocomplete popup keyboard usage test";
let HUD;

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(aHud) {
  HUD = aHud;

  content.wrappedJSObject.foobarBug585991 = {
    "item0": "value0",
    "item1": "value1",
    "item2": "value2",
    "item3": "value3",
  };

  let jsterm = HUD.jsterm;
  let popup = jsterm.autocompletePopup;
  let completeNode = jsterm.completeNode;

  ok(!popup.isOpen, "popup is not open");

  popup._panel.addEventListener("popupshown", function onShown() {
    popup._panel.removeEventListener("popupshown", onShown, false);

    ok(popup.isOpen, "popup is open");

    // 4 values, and the following properties:
    // __defineGetter__  __defineSetter__ __lookupGetter__ __lookupSetter__
    // hasOwnProperty isPrototypeOf propertyIsEnumerable toLocaleString toString
    // toSource unwatch valueOf watch constructor.
    is(popup.itemCount, 18, "popup.itemCount is correct");

    let sameItems = popup.getItems().reverse().map(function(e) {return e.label;});
    ok(sameItems.every(function(prop, index) {
      return [
        "__defineGetter__",
        "__defineSetter__",
        "__lookupGetter__",
        "__lookupSetter__",
        "constructor",
        "hasOwnProperty",
        "isPrototypeOf",
        "item0",
        "item1",
        "item2",
        "item3",
        "propertyIsEnumerable",
        "toLocaleString",
        "toSource",
        "toString",
        "unwatch",
        "valueOf",
        "watch",
      ][index] === prop}), "getItems returns the items we expect");

    is(popup.selectedIndex, 17,
       "Index of the first item from bottom is selected.");
    EventUtils.synthesizeKey("VK_DOWN", {});
    EventUtils.synthesizeKey("VK_DOWN", {});

    let prefix = jsterm.inputNode.value.replace(/[\S]/g, " ");

    is(popup.selectedIndex, 0, "index 0 is selected");
    is(popup.selectedItem.label, "watch", "watch is selected");
    is(completeNode.value, prefix + "watch",
        "completeNode.value holds watch");

    EventUtils.synthesizeKey("VK_DOWN", {});

    is(popup.selectedIndex, 1, "index 1 is selected");
    is(popup.selectedItem.label, "valueOf", "valueOf is selected");
    is(completeNode.value, prefix + "valueOf",
        "completeNode.value holds valueOf");

    EventUtils.synthesizeKey("VK_UP", {});

    is(popup.selectedIndex, 0, "index 0 is selected");
    is(popup.selectedItem.label, "watch", "watch is selected");
    is(completeNode.value, prefix + "watch",
        "completeNode.value holds watch");

    popup._panel.addEventListener("popuphidden", autocompletePopupHidden, false);

    EventUtils.synthesizeKey("VK_TAB", {});
  }, false);

  jsterm.setInputValue("window.foobarBug585991");
  EventUtils.synthesizeKey(".", {});
}

function autocompletePopupHidden()
{
  let jsterm = HUD.jsterm;
  let popup = jsterm.autocompletePopup;
  let completeNode = jsterm.completeNode;
  let inputNode = jsterm.inputNode;

  popup._panel.removeEventListener("popuphidden", autocompletePopupHidden, false);

  ok(!popup.isOpen, "popup is not open");

  is(inputNode.value, "window.foobarBug585991.watch",
     "completion was successful after VK_TAB");

  ok(!completeNode.value, "completeNode is empty");

  popup._panel.addEventListener("popupshown", function onShown() {
    popup._panel.removeEventListener("popupshown", onShown, false);

    ok(popup.isOpen, "popup is open");

    is(popup.itemCount, 18, "popup.itemCount is correct");

    is(popup.selectedIndex, 17, "First index from bottom is selected");
    EventUtils.synthesizeKey("VK_DOWN", {});
    EventUtils.synthesizeKey("VK_DOWN", {});

    let prefix = jsterm.inputNode.value.replace(/[\S]/g, " ");

    is(popup.selectedIndex, 0, "index 0 is selected");
    is(popup.selectedItem.label, "watch", "watch is selected");
    is(completeNode.value, prefix + "watch",
        "completeNode.value holds watch");

    popup._panel.addEventListener("popuphidden", function onHidden() {
      popup._panel.removeEventListener("popuphidden", onHidden, false);

      ok(!popup.isOpen, "popup is not open after VK_ESCAPE");

      is(inputNode.value, "window.foobarBug585991.",
         "completion was cancelled");

      ok(!completeNode.value, "completeNode is empty");

      executeSoon(testReturnKey);
    }, false);

    executeSoon(function() {
      EventUtils.synthesizeKey("VK_ESCAPE", {});
    });
  }, false);

  executeSoon(function() {
    jsterm.setInputValue("window.foobarBug585991");
    EventUtils.synthesizeKey(".", {});
  });
}

function testReturnKey()
{
  let jsterm = HUD.jsterm;
  let popup = jsterm.autocompletePopup;
  let completeNode = jsterm.completeNode;
  let inputNode = jsterm.inputNode;

  popup._panel.addEventListener("popupshown", function onShown() {
    popup._panel.removeEventListener("popupshown", onShown, false);

    ok(popup.isOpen, "popup is open");

    is(popup.itemCount, 18, "popup.itemCount is correct");

    is(popup.selectedIndex, 17, "First index from bottom is selected");
    EventUtils.synthesizeKey("VK_DOWN", {});
    EventUtils.synthesizeKey("VK_DOWN", {});

    let prefix = jsterm.inputNode.value.replace(/[\S]/g, " ");

    is(popup.selectedIndex, 0, "index 0 is selected");
    is(popup.selectedItem.label, "watch", "watch is selected");
    is(completeNode.value, prefix + "watch",
        "completeNode.value holds watch");

    EventUtils.synthesizeKey("VK_DOWN", {});

    is(popup.selectedIndex, 1, "index 1 is selected");
    is(popup.selectedItem.label, "valueOf", "valueOf is selected");
    is(completeNode.value, prefix + "valueOf",
        "completeNode.value holds valueOf");

    popup._panel.addEventListener("popuphidden", function onHidden() {
      popup._panel.removeEventListener("popuphidden", onHidden, false);

      ok(!popup.isOpen, "popup is not open after VK_RETURN");

      is(inputNode.value, "window.foobarBug585991.valueOf",
         "completion was successful after VK_RETURN");

      ok(!completeNode.value, "completeNode is empty");

      dontShowArrayNumbers();
    }, false);

    EventUtils.synthesizeKey("VK_RETURN", {});
  }, false);

  executeSoon(function() {
    jsterm.setInputValue("window.foobarBug58599");
    EventUtils.synthesizeKey("1", {});
    EventUtils.synthesizeKey(".", {});
  });
}

function dontShowArrayNumbers()
{
  content.wrappedJSObject.foobarBug585991 = ["Sherlock Holmes"];

  let jsterm = HUD.jsterm;
  let popup = jsterm.autocompletePopup;
  let completeNode = jsterm.completeNode;

  popup._panel.addEventListener("popupshown", function onShown() {
    popup._panel.removeEventListener("popupshown", onShown, false);

    let sameItems = popup.getItems().map(function(e) {return e.label;});
    ok(!sameItems.some(function(prop, index) { prop === "0"; }),
       "Completing on an array doesn't show numbers.");

    popup._panel.addEventListener("popuphidden", consoleOpened, false);

    EventUtils.synthesizeKey("VK_TAB", {});

    executeSoon(finishTest);
  }, false);

  jsterm.setInputValue("window.foobarBug585991");
  EventUtils.synthesizeKey(".", {});
}

