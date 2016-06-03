/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test pretty printing source mapped sources.

var gClient;
var gThreadClient;
var gSource;

var gTab, gPanel, gClient, gThreadClient, gSource;

const TAB_URL = EXAMPLE_URL + "doc_pretty-print-2.html";

function test() {
  let options = {
    source: EXAMPLE_URL + "code_ugly-2.js",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
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
  callInTab(gTab, "a");
}

function testDbgStatement(event, { frame, why }) {
  is(why.type, "debuggerStatement");
  const { source, line } = frame.where;
  is(source.url, B_URL);
  is(line, 2);

  disablePrettyPrint();
}

function disablePrettyPrint() {
  gThreadClient.source(gSource).disablePrettyPrint(testUgly);
}

function testUgly({ error, source }) {
  ok(!error);
  ok(!source.includes("\n  "));
  getFrame();
}

function getFrame() {
  gThreadClient.getFrames(0, 1, testFrame);
}

function testFrame({ frames: [frame] }) {
  const { source, line } = frame.where;
  is(source.url, B_URL);
  is(line, 1);

  resumeDebuggerThenCloseAndFinish(gPanel);
}

registerCleanupFunction(function () {
  gTab = gPanel = gClient = gThreadClient = null;
});
