/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function* () {
  let newWindowPromise = BrowserTestUtils.waitForNewWindow();
  window.open("data:text/html;charset=utf-8,", "_blank");
  let newWindow = yield newWindowPromise;

  newWindow.focus();
  yield once(newWindow.gBrowser, "load", true);

  let tab = newWindow.gBrowser.selectedTab;
  yield openRDM(tab);

  // Close the window on a tab with an active responsive design UI and
  // wait for the UI to gracefully shutdown.  This has leaked the window
  // in the past.
  ok(ResponsiveUIManager.isActiveForTab(tab),
     "ResponsiveUI should be active for tab when the window is closed");
  let offPromise = once(ResponsiveUIManager, "off");
  yield BrowserTestUtils.closeWindow(newWindow);
  yield offPromise;
});
