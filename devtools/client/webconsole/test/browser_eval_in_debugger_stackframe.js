/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that makes sure web console eval happens in the user-selected stackframe
// from the js debugger.

"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/" +
                 "test/test-eval-in-stackframe.html";

var gWebConsole, gJSTerm, gDebuggerWin, gThread, gDebuggerController;
var gStackframes;

function test() {
  loadTab(TEST_URI).then(() => {
    openConsole().then(consoleOpened);
  });
}

function consoleOpened(hud) {
  gWebConsole = hud;
  gJSTerm = hud.jsterm;
  gJSTerm.execute("foo").then(onExecuteFoo);
}

function onExecuteFoo() {
  isnot(gWebConsole.outputNode.textContent.indexOf("globalFooBug783499"), -1,
        "|foo| value is correct");

  gJSTerm.clearOutput();

  // Test for Bug 690529 - Web Console and Scratchpad should evaluate
  // expressions in the scope of the content window, not in a sandbox.
  executeSoon(() => {
    gJSTerm.execute("foo2 = 'newFoo'; window.foo2").then(onNewFoo2);
  });
}

function onNewFoo2(msg) {
  is(gWebConsole.outputNode.textContent.indexOf("undefined"), -1,
     "|undefined| is not displayed after adding |foo2|");

  ok(msg, "output result found");

  isnot(msg.textContent.indexOf("newFoo"), -1,
        "'newFoo' is displayed after adding |foo2|");

  gJSTerm.clearOutput();

  info("openDebugger");
  executeSoon(() => openDebugger().then(debuggerOpened));
}

function debuggerOpened(aResult) {
  gDebuggerWin = aResult.panelWin;
  gDebuggerController = gDebuggerWin.DebuggerController;
  gThread = gDebuggerController.activeThread;
  gStackframes = gDebuggerController.StackFrames;

  info("openConsole");
  executeSoon(() =>
    openConsole().then(() =>
      gJSTerm.execute("foo + foo2").then(onExecuteFooAndFoo2)
    )
  );
}

function onExecuteFooAndFoo2() {
  let expected = "globalFooBug783499newFoo";
  isnot(gWebConsole.outputNode.textContent.indexOf(expected), -1,
        "|foo + foo2| is displayed after starting the debugger");

  executeSoon(() => {
    gJSTerm.clearOutput();

    info("openDebugger");
    openDebugger().then(() => {
      gThread.addOneTimeListener("framesadded", onFramesAdded);

      info("firstCall()");
      content.wrappedJSObject.firstCall();
    });
  });
}

function onFramesAdded() {
  info("onFramesAdded, openConsole() now");
  executeSoon(() =>
    openConsole().then(() =>
      gJSTerm.execute("foo + foo2").then(onExecuteFooAndFoo2InSecondCall)
    )
  );
}

function onExecuteFooAndFoo2InSecondCall() {
  let expected = "globalFooBug783499foo2SecondCall";
  isnot(gWebConsole.outputNode.textContent.indexOf(expected), -1,
        "|foo + foo2| from |secondCall()|");

  function runOpenConsole() {
    openConsole().then(() => {
      gJSTerm.execute("foo + foo2 + foo3").then(onExecuteFoo23InFirstCall);
    });
  }

  executeSoon(() => {
    gJSTerm.clearOutput();

    info("openDebugger and selectFrame(1)");

    openDebugger().then(() => {
      gStackframes.selectFrame(1);

      info("openConsole");
      executeSoon(() => runOpenConsole());
    });
  });
}

function onExecuteFoo23InFirstCall() {
  let expected = "fooFirstCallnewFoofoo3FirstCall";
  isnot(gWebConsole.outputNode.textContent.indexOf(expected), -1,
        "|foo + foo2 + foo3| from |firstCall()|");

  executeSoon(() =>
    gJSTerm.execute("foo = 'abba'; foo3 = 'bug783499'; foo + foo3").then(
                    onExecuteFooAndFoo3ChangesInFirstCall));
}

function onExecuteFooAndFoo3ChangesInFirstCall() {
  let expected = "abbabug783499";
  isnot(gWebConsole.outputNode.textContent.indexOf(expected), -1,
        "|foo + foo3| updated in |firstCall()|");

  is(content.wrappedJSObject.foo, "globalFooBug783499",
     "|foo| in content window");
  is(content.wrappedJSObject.foo2, "newFoo", "|foo2| in content window");
  ok(!content.wrappedJSObject.foo3,
     "|foo3| was not added to the content window");

  gWebConsole = gJSTerm = gDebuggerWin = gThread = gDebuggerController =
    gStackframes = null;
  executeSoon(finishTest);
}
