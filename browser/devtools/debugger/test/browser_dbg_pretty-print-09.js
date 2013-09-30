/* -*- Mode: javascript; js-indent-level: 2; -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test pretty printing source mapped sources.

var gDebuggee;
var gClient;
var gThreadClient;
var gSource;

let gTab, gDebuggee, gPanel, gClient, gThreadClient;

const TAB_URL = EXAMPLE_URL + "doc_pretty-print-2.html";

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gClient = gPanel.panelWin.gClient;
    gThreadClient = gPanel.panelWin.DebuggerController.activeThread;

    findSource();
  });
}

const dataUrl = s => "data:text/javascript," + s;

// These should match the instructions in code_ugly-4.js.
const A = "function a(){b()}";
const A_URL = dataUrl(A);
const B = "function b(){debugger}";
const B_URL = dataUrl(B);

function findSource() {
  gThreadClient.getSources(({ error, sources }) => {
    ok(!error);
    sources = sources.filter(s => s.url === B_URL);
    is(sources.length, 1);
    prettyPrint(sources[0]);
  });
}

function prettyPrint(source) {
  gThreadClient.source(source).prettyPrint(2, runCode);
}

function runCode({ error }) {
  ok(!error);
  gClient.addOneTimeListener("paused", testDbgStatement);
  gDebuggee.a();
}

function testDbgStatement(event, { frame, why }) {
  dump("FITZGEN: inside testDbgStatement\n");

  try {
    is(why.type, "debuggerStatement");
    const { url, line, column } = frame.where;
    is(url, B_URL);
    is(line, 2);
    is(column, 2);

    resumeDebuggerThenCloseAndFinish(gPanel);
  } catch (e) {
    dump("FITZGEN: got an error! " + DevToolsUtils.safeErrorString(e) + "\n");
  }
}

registerCleanupFunction(function() {
  gTab = gDebuggee = gPanel = gClient = gThreadClient = null;
});
