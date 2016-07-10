/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that document.body autocompletes in the web console.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console autocompletion " +
                 "bug in document.body";

var gHUD;

add_task(function* () {
  yield loadTab(TEST_URI);

  gHUD = yield openConsole();

  yield consoleOpened();
  yield autocompletePopupHidden();
  let view = yield testPropertyPanel();
  yield onVariablesViewReady(view);

  gHUD = null;
});

function consoleOpened() {
  let deferred = promise.defer();

  let jsterm = gHUD.jsterm;
  let popup = jsterm.autocompletePopup;

  ok(!popup.isOpen, "popup is not open");

  popup.once("popup-opened", () => {
    ok(popup.isOpen, "popup is open");

    is(popup.itemCount, jsterm._autocompleteCache.length,
       "popup.itemCount is correct");
    isnot(jsterm._autocompleteCache.indexOf("addEventListener"), -1,
          "addEventListener is in the list of suggestions");
    isnot(jsterm._autocompleteCache.indexOf("bgColor"), -1,
          "bgColor is in the list of suggestions");
    isnot(jsterm._autocompleteCache.indexOf("ATTRIBUTE_NODE"), -1,
          "ATTRIBUTE_NODE is in the list of suggestions");

    popup.once("popup-closed", () => {
      deferred.resolve();
    });
    EventUtils.synthesizeKey("VK_ESCAPE", {});
  });

  jsterm.setInputValue("document.body");
  EventUtils.synthesizeKey(".", {});

  return deferred.promise;
}

function autocompletePopupHidden() {
  let deferred = promise.defer();

  let jsterm = gHUD.jsterm;
  let popup = jsterm.autocompletePopup;
  let completeNode = jsterm.completeNode;

  ok(!popup.isOpen, "popup is not open");

  jsterm.once("autocomplete-updated", function () {
    is(completeNode.value, testStr + "dy", "autocomplete shows document.body");
    deferred.resolve();
  });

  let inputStr = "document.b";
  jsterm.setInputValue(inputStr);
  EventUtils.synthesizeKey("o", {});
  let testStr = inputStr.replace(/./g, " ") + " ";

  return deferred.promise;
}

function testPropertyPanel() {
  let deferred = promise.defer();

  let jsterm = gHUD.jsterm;
  jsterm.clearOutput();
  jsterm.execute("document", (msg) => {
    jsterm.once("variablesview-fetched", (evt, view) => {
      deferred.resolve(view);
    });
    let anchor = msg.querySelector(".message-body a");
    EventUtils.synthesizeMouse(anchor, 2, 2, {}, gHUD.iframeWindow);
  });

  return deferred.promise;
}

function onVariablesViewReady(view) {
  return findVariableViewProperties(view, [
    { name: "body", value: "<body>" },
  ], { webconsole: gHUD });
}
