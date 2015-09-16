/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<p>bug 585991 - autocomplete " +
                 "popup keyboard usage test";
var HUD, popup, jsterm, inputNode, completeNode;

var test = asyncTest(function*() {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();

  yield consoleOpened(hud);
  yield popupHideAfterTab();
  yield testReturnKey();
  yield dontShowArrayNumbers();
  yield testReturnWithNoSelection();
  yield popupHideAfterReturnWithNoSelection();
  yield testCompletionInText();
  yield popupHideAfterCompletionInText();

  HUD = popup = jsterm = inputNode = completeNode = null;
});

var consoleOpened = Task.async(function*(aHud) {
  let deferred = promise.defer();
  HUD = aHud;
  info("web console opened");

  jsterm = HUD.jsterm;

  yield jsterm.execute("window.foobarBug585991={" +
    "'item0': 'value0'," +
    "'item1': 'value1'," +
    "'item2': 'value2'," +
    "'item3': 'value3'" +
  "}");
  yield jsterm.execute("window.testBug873250a = 'hello world';"
    + "window.testBug873250b = 'hello world 2';");
  popup = jsterm.autocompletePopup;
  completeNode = jsterm.completeNode;
  inputNode = jsterm.inputNode;

  ok(!popup.isOpen, "popup is not open");

  popup._panel.addEventListener("popupshown", function onShown() {
    popup._panel.removeEventListener("popupshown", onShown, false);

    ok(popup.isOpen, "popup is open");

    // 4 values, and the following properties:
    // __defineGetter__  __defineSetter__ __lookupGetter__ __lookupSetter__
    // __proto__ hasOwnProperty isPrototypeOf propertyIsEnumerable
    // toLocaleString toString toSource unwatch valueOf watch constructor.
    is(popup.itemCount, 19, "popup.itemCount is correct");

    let sameItems = popup.getItems().reverse().map(function(e) {
      return e.label;
    });
    ok(sameItems.every(function(prop, index) {
      return [
        "__defineGetter__",
        "__defineSetter__",
        "__lookupGetter__",
        "__lookupSetter__",
        "__proto__",
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
      ][index] === prop;
    }), "getItems returns the items we expect");

    is(popup.selectedIndex, 18,
       "Index of the first item from bottom is selected.");
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

    let currentSelectionIndex = popup.selectedIndex;

    EventUtils.synthesizeKey("VK_PAGE_DOWN", {});

    ok(popup.selectedIndex > currentSelectionIndex,
      "Index is greater after PGDN");

    currentSelectionIndex = popup.selectedIndex;
    EventUtils.synthesizeKey("VK_PAGE_UP", {});

    ok(popup.selectedIndex < currentSelectionIndex,
       "Index is less after Page UP");

    EventUtils.synthesizeKey("VK_END", {});
    is(popup.selectedIndex, 18, "index is last after End");

    EventUtils.synthesizeKey("VK_HOME", {});
    is(popup.selectedIndex, 0, "index is first after Home");

    info("press Tab and wait for popup to hide");
    popup._panel.addEventListener("popuphidden", function popupHidden() {
      popup._panel.removeEventListener("popuphidden", popupHidden, false);
      deferred.resolve();
    }, false);
    EventUtils.synthesizeKey("VK_TAB", {});
  }, false);

  info("wait for completion: window.foobarBug585991.");
  jsterm.setInputValue("window.foobarBug585991");
  EventUtils.synthesizeKey(".", {});

  return deferred.promise;
});

function popupHideAfterTab() {
  let deferred = promise.defer();

  // At this point the completion suggestion should be accepted.
  ok(!popup.isOpen, "popup is not open");

  is(inputNode.value, "window.foobarBug585991.watch",
     "completion was successful after VK_TAB");

  ok(!completeNode.value, "completeNode is empty");

  popup._panel.addEventListener("popupshown", function onShown() {
    popup._panel.removeEventListener("popupshown", onShown, false);

    ok(popup.isOpen, "popup is open");

    is(popup.itemCount, 19, "popup.itemCount is correct");

    is(popup.selectedIndex, 18, "First index from bottom is selected");
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

      deferred.resolve();
    }, false);

    info("press Escape to close the popup");
    executeSoon(function() {
      EventUtils.synthesizeKey("VK_ESCAPE", {});
    });
  }, false);

  info("wait for completion: window.foobarBug585991.");
  executeSoon(function() {
    jsterm.setInputValue("window.foobarBug585991");
    EventUtils.synthesizeKey(".", {});
  });

  return deferred.promise;
}

function testReturnKey() {
  let deferred = promise.defer();

  popup._panel.addEventListener("popupshown", function onShown() {
    popup._panel.removeEventListener("popupshown", onShown, false);

    ok(popup.isOpen, "popup is open");

    is(popup.itemCount, 19, "popup.itemCount is correct");

    is(popup.selectedIndex, 18, "First index from bottom is selected");
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

      deferred.resolve();
    }, false);

    info("press Return to accept suggestion. wait for popup to hide");

    executeSoon(() => EventUtils.synthesizeKey("VK_RETURN", {}));
  }, false);

  info("wait for completion suggestions: window.foobarBug585991.");

  executeSoon(function() {
    jsterm.setInputValue("window.foobarBug58599");
    EventUtils.synthesizeKey("1", {});
    EventUtils.synthesizeKey(".", {});
  });

  return deferred.promise;
}

function dontShowArrayNumbers() {
  let deferred = promise.defer();

  info("dontShowArrayNumbers");
  content.wrappedJSObject.foobarBug585991 = ["Sherlock Holmes"];

  jsterm = HUD.jsterm;
  popup = jsterm.autocompletePopup;

  popup._panel.addEventListener("popupshown", function onShown() {
    popup._panel.removeEventListener("popupshown", onShown, false);

    let sameItems = popup.getItems().map(function(e) {
      return e.label;
    });
    ok(!sameItems.some(function(prop) {
      prop === "0";
    }), "Completing on an array doesn't show numbers.");

    popup._panel.addEventListener("popuphidden", function popupHidden() {
      popup._panel.removeEventListener("popuphidden", popupHidden, false);
      deferred.resolve();
    }, false);

    info("wait for popup to hide");
    executeSoon(() => EventUtils.synthesizeKey("VK_ESCAPE", {}));
  }, false);

  info("wait for popup to show");
  executeSoon(() => {
    jsterm.setInputValue("window.foobarBug585991");
    EventUtils.synthesizeKey(".", {});
  });

  return deferred.promise;
}

function testReturnWithNoSelection() {
  let deferred = promise.defer();

  info("test pressing return with open popup, but no selection, see bug 873250");

  popup._panel.addEventListener("popupshown", function onShown() {
    popup._panel.removeEventListener("popupshown", onShown);

    ok(popup.isOpen, "popup is open");
    is(popup.itemCount, 2, "popup.itemCount is correct");
    isnot(popup.selectedIndex, -1, "popup.selectedIndex is correct");

    info("press Return and wait for popup to hide");
    popup._panel.addEventListener("popuphidden", function popupHidden() {
      popup._panel.removeEventListener("popuphidden", popupHidden);
      deferred.resolve();
    });
    executeSoon(() => EventUtils.synthesizeKey("VK_RETURN", {}));
  });

  executeSoon(() => {
    info("wait for popup to show");
    jsterm.setInputValue("window.testBu");
    EventUtils.synthesizeKey("g", {});
  });

  return deferred.promise;
}

function popupHideAfterReturnWithNoSelection() {
  ok(!popup.isOpen, "popup is not open after VK_RETURN");

  is(inputNode.value, "", "inputNode is empty after VK_RETURN");
  is(completeNode.value, "", "completeNode is empty");
  is(jsterm.history[jsterm.history.length - 1], "window.testBug",
     "jsterm history is correct");

  return promise.resolve();
}

function testCompletionInText() {
  info("test that completion works inside text, see bug 812618");

  let deferred = promise.defer();

  popup._panel.addEventListener("popupshown", function onShown() {
    popup._panel.removeEventListener("popupshown", onShown);

    ok(popup.isOpen, "popup is open");
    is(popup.itemCount, 2, "popup.itemCount is correct");

    EventUtils.synthesizeKey("VK_DOWN", {});
    is(popup.selectedIndex, 0, "popup.selectedIndex is correct");
    ok(!completeNode.value, "completeNode.value is empty");

    let items = popup.getItems().reverse().map(e => e.label);
    let sameItems = items.every((prop, index) =>
      ["testBug873250a", "testBug873250b"][index] === prop);
    ok(sameItems, "getItems returns the items we expect");

    info("press Tab and wait for popup to hide");
    popup._panel.addEventListener("popuphidden", function popupHidden() {
      popup._panel.removeEventListener("popuphidden", popupHidden);
      deferred.resolve();
    });
    EventUtils.synthesizeKey("VK_TAB", {});
  });

  jsterm.setInputValue("dump(window.testBu)");
  inputNode.selectionStart = inputNode.selectionEnd = 18;
  EventUtils.synthesizeKey("g", {});
  return deferred.promise;
}

function popupHideAfterCompletionInText() {
  // At this point the completion suggestion should be accepted.
  ok(!popup.isOpen, "popup is not open");
  is(inputNode.value, "dump(window.testBug873250b)",
     "completion was successful after VK_TAB");
  is(inputNode.selectionStart, 26, "cursor location is correct");
  is(inputNode.selectionStart, inputNode.selectionEnd,
     "cursor location (confirmed)");
  ok(!completeNode.value, "completeNode is empty");

  return promise.resolve();
}
