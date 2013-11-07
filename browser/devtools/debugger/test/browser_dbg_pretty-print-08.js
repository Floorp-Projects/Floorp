/* -*- Mode: javascript; js-indent-level: 2; -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test stepping through pretty printed sources.

let gTab, gDebuggee, gPanel, gClient, gThreadClient, gSource;

const TAB_URL = EXAMPLE_URL + "doc_pretty-print-2.html";

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gClient = gPanel.panelWin.gClient;
    gThreadClient = gPanel.panelWin.DebuggerController.activeThread;

    gDebuggee.noop = x => x;
    findSource();
  });
}

let CODE_URL;

const BP_LOCATION = {
  line: 5,
  column: 11
};

function findSource() {
  gThreadClient.getSources(({ error, sources }) => {
    ok(!error);
    sources = sources.filter(s => s.url.contains("code_ugly-3.js"));
    is(sources.length, 1);
    [gSource] = sources;
    CODE_URL = BP_LOCATION.url = gSource.url;

    prettyPrintSource(sources[0]);
  });
}

function prettyPrintSource(source) {
  gThreadClient.source(gSource).prettyPrint(2, runCode);
}

function runCode({ error }) {
  ok(!error);
  gClient.addOneTimeListener("paused", testDbgStatement);
  gDebuggee.main3();
}

function testDbgStatement(event, { why, frame }) {
  is(why.type, "debuggerStatement");
  const { url, line, column } = frame.where;
  is(url, CODE_URL);
  is(line, 3);
  setBreakpoint();
}

function setBreakpoint() {
  gThreadClient.setBreakpoint(BP_LOCATION, ({ error, actualLocation }) => {
    ok(!error);
    ok(!actualLocation);
    testStepping();
  });
}

function testStepping() {
  gClient.addOneTimeListener("paused", (event, { why, frame }) => {
    is(why.type, "resumeLimit");
    const { url, line } = frame.where;
    is(url, CODE_URL);
    is(line, 4);
    testHitBreakpoint();
  });
  gThreadClient.stepIn();
}

function testHitBreakpoint() {
  gClient.addOneTimeListener("paused", (event, { why, frame }) => {
    is(why.type, "breakpoint");
    const { url, line } = frame.where;
    is(url, CODE_URL);
    is(line, BP_LOCATION.line);

    resumeDebuggerThenCloseAndFinish(gPanel);
  });
  gThreadClient.resume();
}

registerCleanupFunction(function() {
  gTab = gDebuggee = gPanel = gClient = gThreadClient = gSource = null;
});
