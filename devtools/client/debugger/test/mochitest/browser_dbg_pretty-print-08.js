/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test stepping through pretty printed sources.

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

const BP_LOCATION = {
  line: 5,
  // column: 0
};

function findSource() {
  gThreadClient.getSources(({ error, sources }) => {
    ok(!error, "error should exist");
    sources = sources.filter(s => s.url.includes("code_ugly-3.js"));
    is(sources.length, 1, "sources.length should be 1");
    [gSource] = sources;
    BP_LOCATION.actor = gSource.actor;

    prettyPrintSource(sources[0]);
  });
}

function prettyPrintSource(source) {
  gThreadClient.source(gSource).prettyPrint(2, runCode);
}

function runCode({ error }) {
  ok(!error);
  gClient.addOneTimeListener("paused", testDbgStatement);
  callInTab(gTab, "main3");
}

function testDbgStatement(event, { why, frame }) {
  is(why.type, "debuggerStatement");
  const { source, line, column } = frame.where;
  is(source.actor, BP_LOCATION.actor, "source.actor should be the right actor");
  is(line, 3, "the line should be 3");
  setBreakpoint();
}

function setBreakpoint() {
  gThreadClient.source(gSource).setBreakpoint(
    { line: BP_LOCATION.line,
      column: BP_LOCATION.column },
    ({ error, actualLocation }) => {
      ok(!error, "error should not exist");
      ok(!actualLocation, "actualLocation should not exist");
      testStepping();
    }
  );
}

function testStepping() {
  gClient.addOneTimeListener("paused", (event, { why, frame }) => {
    is(why.type, "resumeLimit");
    const { source, line } = frame.where;
    is(source.actor, BP_LOCATION.actor, "source.actor should be the right actor");
    is(line, 4, "the line should be 4");
    testHitBreakpoint();
  });
  gThreadClient.stepIn();
}

function testHitBreakpoint() {
  gClient.addOneTimeListener("paused", (event, { why, frame }) => {
    is(why.type, "breakpoint");
    const { source, line } = frame.where;
    is(source.actor, BP_LOCATION.actor, "source.actor should be the right actor");
    is(line, BP_LOCATION.line, "the line should the right line");

    resumeDebuggerThenCloseAndFinish(gPanel);
  });
  gThreadClient.resume();
}

registerCleanupFunction(function () {
  gTab = gPanel = gClient = gThreadClient = gSource = null;
});
