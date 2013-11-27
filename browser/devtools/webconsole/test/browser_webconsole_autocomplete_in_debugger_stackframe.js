/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that makes sure web console autocomplete happens in the user-selected stackframe
// from the js debugger.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-autocomplete-in-stackframe.html";

let testDriver, gStackframes;

function test()
{
  requestLongerTimeout(2);
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, function(hud) {
      testDriver = testCompletion(hud);
      testDriver.next();
    });
  }, true);
}

function testNext() {
  executeSoon(function() {
    testDriver.next();
  });
}

function testCompletion(hud) {
  let jsterm = hud.jsterm;
  let input = jsterm.inputNode;
  let popup = jsterm.autocompletePopup;

  // Test if 'f' gives 'foo1' but not 'foo2' or 'foo3'
  input.value = "f";
  input.setSelectionRange(1, 1);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield undefined;

  let newItems = popup.getItems();
  ok(newItems.length > 0, "'f' gave a list of suggestions");
  ok(!newItems.every(function(item) {
       return item.label != "foo1";
     }), "autocomplete results do contain foo1");
  ok(!newItems.every(function(item) {
       return item.label != "foo1Obj";
     }), "autocomplete results do contain foo1Obj");
  ok(newItems.every(function(item) {
       return item.label != "foo2";
     }), "autocomplete results do not contain foo2");
  ok(newItems.every(function(item) {
       return item.label != "foo2Obj";
     }), "autocomplete results do not contain foo2Obj");
  ok(newItems.every(function(item) {
       return item.label != "foo3";
     }), "autocomplete results do not contain foo3");
  ok(newItems.every(function(item) {
       return item.label != "foo3Obj";
     }), "autocomplete results do not contain foo3Obj");

  // Test if 'foo1Obj.' gives 'prop1' and 'prop2'
  input.value = "foo1Obj.";
  input.setSelectionRange(8, 8);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield undefined;

  newItems = popup.getItems();
  ok(!newItems.every(function(item) {
       return item.label != "prop1";
     }), "autocomplete results do contain prop1");
  ok(!newItems.every(function(item) {
       return item.label != "prop2";
     }), "autocomplete results do contain prop2");

  // Test if 'foo1Obj.prop2.' gives 'prop21'
  input.value = "foo1Obj.prop2.";
  input.setSelectionRange(14, 14);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield undefined;

  newItems = popup.getItems();
  ok(!newItems.every(function(item) {
       return item.label != "prop21";
     }), "autocomplete results do contain prop21");

  info("openDebugger");
  executeSoon(() => openDebugger().then(debuggerOpened));
  yield undefined;

  // From this point on the
  // Test if 'f' gives 'foo3' and 'foo1' but not 'foo2'
  input.value = "f";
  input.setSelectionRange(1, 1);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield undefined;

  newItems = popup.getItems();
  ok(newItems.length > 0, "'f' gave a list of suggestions");
  ok(!newItems.every(function(item) {
       return item.label != "foo3";
     }), "autocomplete results do contain foo3");
  ok(!newItems.every(function(item) {
       return item.label != "foo3Obj";
     }), "autocomplete results do contain foo3Obj");
  ok(!newItems.every(function(item) {
       return item.label != "foo1";
     }), "autocomplete results do contain foo1");
  ok(!newItems.every(function(item) {
       return item.label != "foo1Obj";
     }), "autocomplete results do contain foo1Obj");
  ok(newItems.every(function(item) {
       return item.label != "foo2";
     }), "autocomplete results do not contain foo2");
  ok(newItems.every(function(item) {
       return item.label != "foo2Obj";
     }), "autocomplete results do not contain foo2Obj");

  openDebugger().then(() => {
    gStackframes.selectFrame(1);

    info("openConsole");
    executeSoon(() => openConsole(null, () => testDriver.next()));
  });
  yield undefined;

  // Test if 'f' gives 'foo2' and 'foo1' but not 'foo3'
  input.value = "f";
  input.setSelectionRange(1, 1);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield undefined;

  newItems = popup.getItems();
  ok(newItems.length > 0, "'f' gave a list of suggestions");
  ok(!newItems.every(function(item) {
       return item.label != "foo2";
     }), "autocomplete results do contain foo2");
  ok(!newItems.every(function(item) {
       return item.label != "foo2Obj";
     }), "autocomplete results do contain foo2Obj");
  ok(!newItems.every(function(item) {
       return item.label != "foo1";
     }), "autocomplete results do contain foo1");
  ok(!newItems.every(function(item) {
       return item.label != "foo1Obj";
     }), "autocomplete results do contain foo1Obj");
  ok(newItems.every(function(item) {
       return item.label != "foo3";
     }), "autocomplete results do not contain foo3");
  ok(newItems.every(function(item) {
       return item.label != "foo3Obj";
     }), "autocomplete results do not contain foo3Obj");

  // Test if 'foo2Obj.' gives 'prop1'
  input.value = "foo2Obj.";
  input.setSelectionRange(8, 8);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield undefined;

  newItems = popup.getItems();
  ok(!newItems.every(function(item) {
       return item.label != "prop1";
     }), "autocomplete results do contain prop1");

  // Test if 'foo1Obj.prop1.' gives 'prop11'
  input.value = "foo2Obj.prop1.";
  input.setSelectionRange(14, 14);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield undefined;

  newItems = popup.getItems();
  ok(!newItems.every(function(item) {
       return item.label != "prop11";
     }), "autocomplete results do contain prop11");

  // Test if 'foo1Obj.prop1.prop11.' gives suggestions for a string i.e. 'length'
  input.value = "foo2Obj.prop1.prop11.";
  input.setSelectionRange(21, 21);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield undefined;

  newItems = popup.getItems();
  ok(!newItems.every(function(item) {
       return item.label != "length";
     }), "autocomplete results do contain length");

  testDriver = null;
  executeSoon(finishTest);
  yield undefined;
}

function debuggerOpened(aResult)
{
  let debuggerWin = aResult.panelWin;
  let debuggerController = debuggerWin.DebuggerController;
  let thread = debuggerController.activeThread;
  gStackframes = debuggerController.StackFrames;

  executeSoon(() => {
    thread.addOneTimeListener("framesadded", onFramesAdded);
    info("firstCall()");
    content.wrappedJSObject.firstCall();
  });
}

function onFramesAdded()
{
  info("onFramesAdded, openConsole() now");
  executeSoon(() => openConsole(null, testNext));
}
