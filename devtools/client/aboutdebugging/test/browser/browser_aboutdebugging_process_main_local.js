/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-collapsibilities.js",
  this
);

// Test for main process against This Firefox.
//
// The main added value for this test is to check that listing processes
// and opening a toolbox targeting a process works, even though debugging
// the main process of This Firefox is not really supported.
add_task(async function() {
  await pushPref("devtools.aboutdebugging.process-debugging", true);
  await pushPref("devtools.aboutdebugging.test-local-process-debugging", true);

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  info("Check debug target item of the main process");
  const mainProcessItem = findDebugTargetByText("Main Process", document);
  ok(mainProcessItem, "Debug target item of the main process should display");
  ok(
    mainProcessItem.textContent.includes("Main Process for the target browser"),
    "Debug target item of the main process should contains the description"
  );

  info("Inspect main process and wait for DevTools to open");
  const { devtoolsTab } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    "Main Process"
  );

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
  await removeTab(tab);
});
