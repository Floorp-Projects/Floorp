/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Bug 1898490: DevTools may prevents opening when some random content process is being destroyed
// in middle of DevTools initialization.
// So try opening DevTools a couple of times while destroying content processes in the background.

const URL =
  "data:text/html;charset=utf8,test many toggles with other content process destructions";

add_task(async function () {
  const tab = await addTab(URL);

  const ProcessTools = Cc["@mozilla.org/processtools-service;1"].getService(
    Ci.nsIProcessToolsService
  );

  const openedTabs = [];
  const interval = setInterval(() => {
    // Close the process specific to about:home, which is using a privilegedabout process type
    const pid = ChromeUtils.getAllDOMProcesses().filter(
      r => r.remoteType == "privilegedabout"
    )[0]?.osPid;
    if (!pid) {
      return;
    }
    ProcessTools.kill(pid);
    // The privilegedabout process wouldn't be automatically re-created, so open a new tab to force creating a new process.
    openedTabs.push(BrowserTestUtils.addTab(gBrowser, "about:home"));
  });

  info(
    "Open/close DevTools many times in a row while some processes get destroyed"
  );
  for (let i = 0; i < 5; i++) {
    const toolbox = await gDevTools.showToolboxForTab(tab, {
      toolId: "webconsole",
    });
    await toolbox.destroy();
  }

  clearInterval(interval);

  info("Close all tabs that were used to spawn a new content process");
  for (const tab of openedTabs) {
    await removeTab(tab);
  }
});
