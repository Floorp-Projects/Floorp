/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the debugger is updated with the correct sources when moving
 * back and forward in the tab.
 */

const TAB_URL_1 = EXAMPLE_URL + "doc_script-switching-01.html";
const TAB_URL_2 = EXAMPLE_URL + "doc_recursion-stack.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gSources;

function test() {
  initDebugger(TAB_URL_1).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;

    testFirstPage()
      .then(testLocationChange)
      .then(testBack)
      .then(testForward)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testFirstPage() {
  info("Testing first page.");

  // Spin the event loop before causing the debuggee to pause, to allow
  // this function to return first.
  executeSoon(() => gDebuggee.firstCall());

  return waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1).then(() => {
    validateFirstPage();
  });
}

function testLocationChange() {
  info("Navigating to a different page.");

  return navigateActiveTabTo(gPanel, TAB_URL_2, gDebugger.EVENTS.SOURCES_ADDED).then(() => {
    validateSecondPage();
  });
}

function testBack() {
  info("Going back.");

  return navigateActiveTabInHistory(gPanel, "back", gDebugger.EVENTS.SOURCES_ADDED).then(() => {
    validateFirstPage();
  });
}

function testForward() {
  info("Going forward.");

  return navigateActiveTabInHistory(gPanel, "forward", gDebugger.EVENTS.SOURCES_ADDED).then(() => {
    validateSecondPage();
  });
}

function validateFirstPage() {
  is(gSources.itemCount, 2,
    "Found the expected number of sources.");
  ok(gSources.getItemForAttachment(e => e.label == "code_script-switching-01.js"),
    "Found the first source label.");
  ok(gSources.getItemForAttachment(e => e.label == "code_script-switching-02.js"),
    "Found the second source label.");
}

function validateSecondPage() {
  is(gSources.itemCount, 1,
    "Found the expected number of sources.");
  ok(gSources.getItemForAttachment(e => e.label == "doc_recursion-stack.html"),
    "Found the single source label.");
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gSources = null;
});
