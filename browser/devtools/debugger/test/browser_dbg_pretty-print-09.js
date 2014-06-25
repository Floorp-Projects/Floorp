/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test pretty printing source mapped sources.

var gDebuggee;
var gClient;
var gThreadClient;
var gSource;

let gTab, gDebuggee, gPanel, gClient, gThreadClient, gSource;

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
    gSource = sources[0];
    prettyPrint();
  });
}

function prettyPrint() {
  gThreadClient.source(gSource).prettyPrint(2, runCode);
}

function runCode({ error }) {
  ok(!error);
  gClient.addOneTimeListener("paused", testDbgStatement);
  gDebuggee.a();
}

function testDbgStatement(event, { frame, why }) {
  is(why.type, "debuggerStatement");
  const { url, line } = frame.where;
  is(url, B_URL);
  is(line, 2);

  disablePrettyPrint();
}

function disablePrettyPrint() {
  gThreadClient.source(gSource).disablePrettyPrint(testUgly);
}

function testUgly({ error, source }) {
  ok(!error);
  ok(!source.contains("\n  "));
  getFrame();
}

function getFrame() {
  gThreadClient.getFrames(0, 1, testFrame);
}

function testFrame({ frames: [frame] }) {
  const { url, line } = frame.where;
  is(url, B_URL);
  is(line, 1);

  resumeDebuggerThenCloseAndFinish(gPanel);
}

registerCleanupFunction(function() {
  gTab = gDebuggee = gPanel = gClient = gThreadClient = null;
});
