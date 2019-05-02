/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the expected supported categories are displayed for USB runtimes.
add_task(async function() {
  const targetTab = await addTab("about:home");

  const { document, tab, window } = await openAboutDebugging();

  // go to This Firefox and inspect the new tab
  info("Inspecting a new tab in This Firefox");
  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  const { devtoolsDocument, devtoolsTab, devtoolsWindow } =
    await openAboutDevtoolsToolbox(document, tab, window, "about:home");
  const targetInfoHeader = devtoolsDocument.querySelector(".qa-debug-target-info");
  ok(targetInfoHeader.textContent.includes("about:home"),
    "about:devtools-toolbox is open for the target");

  // close the inspected tab and check that error page is shown
  info("removing the inspected tab");
  await removeTab(targetTab);
  await waitUntil(() => devtoolsWindow.document.querySelector(".qa-error-page"));

  info("closing the toolbox");
  await removeTab(devtoolsTab);
  info("removing about:debugging tab");
  await removeTab(tab);
});
