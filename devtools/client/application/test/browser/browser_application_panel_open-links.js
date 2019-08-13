/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Toolbox } = require("devtools/client/framework/toolbox");

/**
 * Check that links work when the devtools are detached in a separate window.
 */

const TAB_URL = URL_ROOT + "resources/service-workers/empty.html";

add_task(async function() {
  await enableApplicationPanel();

  const { panel, toolbox } = await openNewTabAndApplicationPanel(TAB_URL);
  const doc = panel.panelWin.document;

  // select service worker view
  selectPage(panel, "service-workers");

  // detach devtools in a separate window
  await toolbox.switchHost(Toolbox.HostType.WINDOW);

  // click on the link and wait for the new tab to open
  const onTabLoaded = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:debugging#workers"
  );
  doc.querySelector(".js-trusted-link").click();
  info("Opening link in a new tab.");
  const newTab = await onTabLoaded;

  // We only need to check that newTab is truthy since
  // BrowserTestUtils.waitForNewTab checks the URL.
  ok(newTab, "The expected tab was opened.");

  info("Wait until the main about debugging container is available");
  await waitUntil(() => {
    const aboutDebuggingDoc = newTab.linkedBrowser.contentDocument;
    return aboutDebuggingDoc.querySelector(".app");
  });

  // close the tab
  info("Closing the tab.");
  await BrowserTestUtils.removeTab(newTab);
});
