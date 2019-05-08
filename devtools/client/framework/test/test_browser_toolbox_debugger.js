/* global toolbox, createDebuggerContext, waitForSources, testUrl,
          waitForPaused, addBreakpoint, assertPausedLocation, stepIn,
          findSource, removeBreakpoint, resume, selectSource */

info(`START: ${new Error().lineNumber}`);

(async function() {
  Services.prefs.clearUserPref("devtools.debugger.tabs");
  Services.prefs.clearUserPref("devtools.debugger.pending-selected-location");

  info("Waiting for debugger load");
  await toolbox.selectTool("jsdebugger");
  const dbg = createDebuggerContext(toolbox);
  const window = dbg.win;
  const document = window.document;

  await waitForSources(dbg, testUrl);

  info("Loaded, selecting the test script to debug");
  // First expand the domain
  const domain = [...document.querySelectorAll(".tree-node")].find(node => {
    return node.querySelector(".label").textContent.trim() == "mozilla.org";
  });
  const arrow = domain.querySelector(".arrow");
  arrow.click();

  const fileName = testUrl.match(/browser-toolbox-test.*\.js/)[0];

  let script = [...document.querySelectorAll(".tree-node")].find(node => {
    return node.textContent.includes(fileName);
  });
  script = script.querySelector(".node");
  script.click();

  const onPaused = waitForPaused(dbg);
  await selectSource(dbg, fileName);
  await addBreakpoint(dbg, fileName, 2);

  await onPaused;

  assertPausedLocation(dbg, fileName, 2);

  await stepIn(dbg);

  assertPausedLocation(dbg, fileName, 3);

  // Remove the breakpoint before resuming in order to prevent hitting the breakpoint
  // again during test closing.
  const source = findSource(dbg, fileName);
  await removeBreakpoint(dbg, source.id, 2);

  await resume(dbg);

  info("Close the browser toolbox");
  toolbox.destroy();
})();
