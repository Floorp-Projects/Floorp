/* global toolbox */

info(`START: ${new Error().lineNumber}`);

let testUrl = "http://mozilla.org/browser-toolbox-test.js";

Task.spawn(function* () {
  Services.prefs.clearUserPref("devtools.debugger.tabs")
  Services.prefs.clearUserPref("devtools.debugger.pending-selected-location")

  info("Waiting for debugger load");
  yield toolbox.selectTool("jsdebugger");
  let dbg = createDebuggerContext(toolbox);
  let window = dbg.win;
  let document = window.document;

  yield waitForSources(dbg, testUrl);
//  yield waitForSourceCount(dbg, 6);

  info("Loaded, selecting the test script to debug");
  // First expand the domain
  let domain = [...document.querySelectorAll(".tree-node")].find(node => {
    return node.textContent == "mozilla.org";
  });
  let arrow = domain.querySelector(".arrow");
  arrow.click();
  let script = [...document.querySelectorAll(".tree-node")].find(node => {
    return node.textContent.includes("browser-toolbox-test.js");
  });
  script = script.querySelector(".node");
  script.click();

  let onPaused = waitForPaused(dbg);
  yield addBreakpoint(dbg, "browser-toolbox-test.js", 2);

  yield onPaused;

  assertPausedLocation(dbg, "browser-toolbox-test.js", 2);

  yield stepIn(dbg);

  assertPausedLocation(dbg, "browser-toolbox-test.js", 3);

  yield resume(dbg);

  info("Close the browser toolbox");
  toolbox.destroy();
});
