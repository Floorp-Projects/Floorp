/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test basic pretty printing functionality. Would be an xpcshell test, except
// for bug 921252.

let gTab, gPanel, gClient, gThreadClient, gSource;

const TAB_URL = EXAMPLE_URL + "doc_pretty-print-2.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gClient = gPanel.panelWin.gClient;
    gThreadClient = gPanel.panelWin.DebuggerController.activeThread;

    findSource();
  });
}

function findSource() {
  gThreadClient.getSources(({ error, sources }) => {
    ok(!error);
    sources = sources.filter(s => s.url.includes('code_ugly-2.js'));
    is(sources.length, 1);
    gSource = sources[0];
    prettyPrintSource();
  });
}

function prettyPrintSource() {
  gThreadClient.source(gSource).prettyPrint(4, testPrettyPrinted);
}

function testPrettyPrinted({ error, source }) {
  ok(!error, "Should not get an error while pretty-printing");
  ok(source.includes("\n    "),
    "Source should be pretty-printed");
  disablePrettyPrint();
}

function disablePrettyPrint() {
  gThreadClient.source(gSource).disablePrettyPrint(testUgly);
}

function testUgly({ error, source }) {
  ok(!error, "Should not get an error while disabling pretty-printing");
  ok(!source.includes("\n    "),
     "Source should not be pretty after disabling pretty-printing");
  closeDebuggerAndFinish(gPanel);
}

registerCleanupFunction(function() {
  gTab = gPanel = gClient = gThreadClient = gSource = null;
});
