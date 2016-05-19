/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that makes sure web console autocomplete happens in the user-selected
// stackframe from the js debugger.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-autocomplete-in-stackframe.html";

var gStackframes;
registerCleanupFunction(function () {
  gStackframes = null;
});

requestLongerTimeout(2);
add_task(function* () {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  yield testCompletion(hud);
});

function* testCompletion(hud) {
  let jsterm = hud.jsterm;
  let input = jsterm.inputNode;
  let popup = jsterm.autocompletePopup;

  // Test that document.title gives string methods. Native getters must execute.
  input.value = "document.title.";
  input.setSelectionRange(input.value.length, input.value.length);
  yield new Promise(resolve => {
    jsterm.complete(jsterm.COMPLETE_HINT_ONLY, resolve);
  });

  let newItems = popup.getItems();
  ok(newItems.length > 0, "'document.title.' gave a list of suggestions");
  ok(newItems.some(function (item) {
    return item.label == "substr";
  }), "autocomplete results do contain substr");
  ok(newItems.some(function (item) {
    return item.label == "toLowerCase";
  }), "autocomplete results do contain toLowerCase");
  ok(newItems.some(function (item) {
    return item.label == "strike";
  }), "autocomplete results do contain strike");

  // Test if 'f' gives 'foo1' but not 'foo2' or 'foo3'
  input.value = "f";
  input.setSelectionRange(1, 1);
  yield new Promise(resolve => {
    jsterm.complete(jsterm.COMPLETE_HINT_ONLY, resolve);
  });

  newItems = popup.getItems();
  ok(newItems.length > 0, "'f' gave a list of suggestions");
  ok(!newItems.every(function (item) {
    return item.label != "foo1";
  }), "autocomplete results do contain foo1");
  ok(!newItems.every(function (item) {
    return item.label != "foo1Obj";
  }), "autocomplete results do contain foo1Obj");
  ok(newItems.every(function (item) {
    return item.label != "foo2";
  }), "autocomplete results do not contain foo2");
  ok(newItems.every(function (item) {
    return item.label != "foo2Obj";
  }), "autocomplete results do not contain foo2Obj");
  ok(newItems.every(function (item) {
    return item.label != "foo3";
  }), "autocomplete results do not contain foo3");
  ok(newItems.every(function (item) {
    return item.label != "foo3Obj";
  }), "autocomplete results do not contain foo3Obj");

  // Test if 'foo1Obj.' gives 'prop1' and 'prop2'
  input.value = "foo1Obj.";
  input.setSelectionRange(8, 8);
  yield new Promise(resolve => {
    jsterm.complete(jsterm.COMPLETE_HINT_ONLY, resolve);
  });

  newItems = popup.getItems();
  ok(!newItems.every(function (item) {
    return item.label != "prop1";
  }), "autocomplete results do contain prop1");
  ok(!newItems.every(function (item) {
    return item.label != "prop2";
  }), "autocomplete results do contain prop2");

  // Test if 'foo1Obj.prop2.' gives 'prop21'
  input.value = "foo1Obj.prop2.";
  input.setSelectionRange(14, 14);
  yield new Promise(resolve => {
    jsterm.complete(jsterm.COMPLETE_HINT_ONLY, resolve);
  });

  newItems = popup.getItems();
  ok(!newItems.every(function (item) {
    return item.label != "prop21";
  }), "autocomplete results do contain prop21");

  info("Opening Debugger");
  let dbg = yield openDebugger();

  info("Waiting for pause");
  yield pauseDebugger(dbg);

  info("Opening Console again");
  yield openConsole();

  // From this point on the
  // Test if 'f' gives 'foo3' and 'foo1' but not 'foo2'
  input.value = "f";
  input.setSelectionRange(1, 1);
  yield new Promise(resolve => {
    jsterm.complete(jsterm.COMPLETE_HINT_ONLY, resolve);
  });

  newItems = popup.getItems();
  ok(newItems.length > 0, "'f' gave a list of suggestions");
  ok(!newItems.every(function (item) {
    return item.label != "foo3";
  }), "autocomplete results do contain foo3");
  ok(!newItems.every(function (item) {
    return item.label != "foo3Obj";
  }), "autocomplete results do contain foo3Obj");
  ok(!newItems.every(function (item) {
    return item.label != "foo1";
  }), "autocomplete results do contain foo1");
  ok(!newItems.every(function (item) {
    return item.label != "foo1Obj";
  }), "autocomplete results do contain foo1Obj");
  ok(newItems.every(function (item) {
    return item.label != "foo2";
  }), "autocomplete results do not contain foo2");
  ok(newItems.every(function (item) {
    return item.label != "foo2Obj";
  }), "autocomplete results do not contain foo2Obj");

  yield openDebugger();

  gStackframes.selectFrame(1);

  info("openConsole");
  yield openConsole();

  // Test if 'f' gives 'foo2' and 'foo1' but not 'foo3'
  input.value = "f";
  input.setSelectionRange(1, 1);
  yield new Promise(resolve => {
    jsterm.complete(jsterm.COMPLETE_HINT_ONLY, resolve);
  });

  newItems = popup.getItems();
  ok(newItems.length > 0, "'f' gave a list of suggestions");
  ok(!newItems.every(function (item) {
    return item.label != "foo2";
  }), "autocomplete results do contain foo2");
  ok(!newItems.every(function (item) {
    return item.label != "foo2Obj";
  }), "autocomplete results do contain foo2Obj");
  ok(!newItems.every(function (item) {
    return item.label != "foo1";
  }), "autocomplete results do contain foo1");
  ok(!newItems.every(function (item) {
    return item.label != "foo1Obj";
  }), "autocomplete results do contain foo1Obj");
  ok(newItems.every(function (item) {
    return item.label != "foo3";
  }), "autocomplete results do not contain foo3");
  ok(newItems.every(function (item) {
    return item.label != "foo3Obj";
  }), "autocomplete results do not contain foo3Obj");

  // Test if 'foo2Obj.' gives 'prop1'
  input.value = "foo2Obj.";
  input.setSelectionRange(8, 8);
  yield new Promise(resolve => {
    jsterm.complete(jsterm.COMPLETE_HINT_ONLY, resolve);
  });

  newItems = popup.getItems();
  ok(!newItems.every(function (item) {
    return item.label != "prop1";
  }), "autocomplete results do contain prop1");

  // Test if 'foo2Obj.prop1.' gives 'prop11'
  input.value = "foo2Obj.prop1.";
  input.setSelectionRange(14, 14);
  yield new Promise(resolve => {
    jsterm.complete(jsterm.COMPLETE_HINT_ONLY, resolve);
  });

  newItems = popup.getItems();
  ok(!newItems.every(function (item) {
    return item.label != "prop11";
  }), "autocomplete results do contain prop11");

  // Test if 'foo2Obj.prop1.prop11.' gives suggestions for a string
  // i.e. 'length'
  input.value = "foo2Obj.prop1.prop11.";
  input.setSelectionRange(21, 21);
  yield new Promise(resolve => {
    jsterm.complete(jsterm.COMPLETE_HINT_ONLY, resolve);
  });

  newItems = popup.getItems();
  ok(!newItems.every(function (item) {
    return item.label != "length";
  }), "autocomplete results do contain length");

  // Test if 'foo1Obj[0].' throws no errors.
  input.value = "foo2Obj[0].";
  input.setSelectionRange(11, 11);
  yield new Promise(resolve => {
    jsterm.complete(jsterm.COMPLETE_HINT_ONLY, resolve);
  });

  newItems = popup.getItems();
  is(newItems.length, 0, "no items for foo2Obj[0]");
}

function pauseDebugger(aResult) {
  let debuggerWin = aResult.panelWin;
  let debuggerController = debuggerWin.DebuggerController;
  let thread = debuggerController.activeThread;
  gStackframes = debuggerController.StackFrames;
  return new Promise(resolve => {
    thread.addOneTimeListener("framesadded", resolve);

    info("firstCall()");
    ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
      content.wrappedJSObject.firstCall();
    });
  });
}
