/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

addRDMTask(
  null,
  async function () {
    const NEW_WINDOW_URL =
      "data:text/html;charset=utf-8,New window opened via window.open";
    const newWindowPromise = BrowserTestUtils.waitForNewWindow({
      // Passing the url param so the Promise will resolve once DOMContentLoaded is emitted
      // on the new window tab
      url: NEW_WINDOW_URL,
    });
    window.open(NEW_WINDOW_URL, "_blank", "noopener,all");

    const newWindow = await newWindowPromise;
    ok(true, "Got new window");

    info("Focus new window");
    newWindow.focus();

    info("Open RDM");
    const tab = newWindow.gBrowser.selectedTab;
    const { ui } = await openRDM(tab);
    await waitForDeviceAndViewportState(ui);

    ok(
      ResponsiveUIManager.isActiveForTab(tab),
      "ResponsiveUI should be active for tab when the window is closed"
    );

    // Close the window on a tab with an active responsive design UI and
    // wait for the UI to gracefully shutdown.  This has leaked the window
    // in the past.
    info("Close the new window");
    const offPromise = once(ResponsiveUIManager, "off");
    await BrowserTestUtils.closeWindow(newWindow);
    await offPromise;
  },
  { onlyPrefAndTask: true }
);
