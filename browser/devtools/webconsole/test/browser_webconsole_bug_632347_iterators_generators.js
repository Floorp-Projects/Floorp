/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-632347-iterators-generators.html";

function test() {
  requestLongerTimeout(6);

  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(HUD) {
  let tools = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools;
  let JSPropertyProvider = tools.require("devtools/toolkit/webconsole/utils").JSPropertyProvider;

  let tmp = Cu.import("resource://gre/modules/jsdebugger.jsm", {});
  tmp.addDebuggerToGlobal(tmp);
  let dbg = new tmp.Debugger;

  let jsterm = HUD.jsterm;
  let win = content.wrappedJSObject;
  let dbgWindow = dbg.makeGlobalObjectReference(win);
  let container = win._container;

  // Make sure autocomplete does not walk through iterators and generators.
  let result = container.gen1.next();
  let completion = JSPropertyProvider(dbgWindow, null, "_container.gen1.");
  isnot(completion.matches.length, 0, "Got matches for gen1");

  is(result+1, container.gen1.next(), "gen1.next() did not execute");

  result = container.gen2.next();

  completion = JSPropertyProvider(dbgWindow, null, "_container.gen2.");
  isnot(completion.matches.length, 0, "Got matches for gen2");

  is((result/2+1)*2, container.gen2.next(),
     "gen2.next() did not execute");

  result = container.iter1.next();
  is(result[0], "foo", "iter1.next() [0] is correct");
  is(result[1], "bar", "iter1.next() [1] is correct");

  completion = JSPropertyProvider(dbgWindow, null, "_container.iter1.");
  isnot(completion.matches.length, 0, "Got matches for iter1");

  result = container.iter1.next();
  is(result[0], "baz", "iter1.next() [0] is correct");
  is(result[1], "baaz", "iter1.next() [1] is correct");

  let dbgContent = dbg.makeGlobalObjectReference(content);
  completion = JSPropertyProvider(dbgContent, null, "_container.iter2.");
  isnot(completion.matches.length, 0, "Got matches for iter2");

  completion = JSPropertyProvider(dbgWindow, null, "window._container.");
  ok(completion, "matches available for window._container");
  ok(completion.matches.length, "matches available for window (length)");

  jsterm.clearOutput();

  jsterm.execute("window._container", (msg) => {
    jsterm.once("variablesview-fetched", testVariablesView.bind(null, HUD));
    let anchor = msg.querySelector(".message-body a");
    EventUtils.synthesizeMouse(anchor, 2, 2, {}, HUD.iframeWindow);
  });
}

function testVariablesView(aWebconsole, aEvent, aView) {
  findVariableViewProperties(aView, [
    { name: "gen1", isGenerator: true },
    { name: "gen2", isGenerator: true },
    { name: "iter1", isIterator: true },
    { name: "iter2", isIterator: true },
  ], { webconsole: aWebconsole }).then(function() {
    executeSoon(finishTest);
  });
}
