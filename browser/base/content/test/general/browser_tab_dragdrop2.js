"use strict";

const ROOT = getRootDirectory(gTestPath);
const URI = ROOT + "browser_tab_dragdrop2_frame1.xul";

// Load the test page (which runs some child popup tests) in a new window.
// After the tests were run, tear off the tab into a new window and run popup
// tests a second time. We don't care about tests results, exceptions and
// crashes will be caught.
add_task(async function() {
  // Open a new window.
  let args = "chrome,all,dialog=no";
  let win = window.openDialog(getBrowserURL(), "_blank", args, URI);

  // Wait until the tests were run.
  await promiseTestsDone(win);
  ok(true, "tests succeeded");

  // Create a second tab so that we can move the original one out.
  win.gBrowser.addTab("about:blank", {skipAnimation: true});

  // Tear off the original tab.
  let browser = win.gBrowser.selectedBrowser;
  let tabClosed = promiseWaitForEvent(browser, "pagehide", true);
  let win2 = win.gBrowser.replaceTabWithWindow(win.gBrowser.tabs[0]);

  // Add a 'TestsDone' event listener to ensure that the docShells is properly
  // swapped to the new window instead of the page being loaded again. If this
  // works fine we should *NOT* see a TestsDone event.
  let onTestsDone = () => ok(false, "shouldn't run tests when tearing off");
  win2.addEventListener("TestsDone", onTestsDone);

  // Wait until the original tab is gone and the new window is ready.
  await Promise.all([tabClosed, promiseDelayedStartupFinished(win2)]);

  // Remove the 'TestsDone' event listener as now
  // we're kicking off a new test run manually.
  win2.removeEventListener("TestsDone", onTestsDone);

  // Run tests once again.
  let promise = promiseTestsDone(win2);
  win2.content.test_panels();
  await promise;
  ok(true, "tests succeeded a second time");

  // Cleanup.
  await promiseWindowClosed(win2);
  await promiseWindowClosed(win);
});

function promiseTestsDone(win) {
  return promiseWaitForEvent(win, "TestsDone");
}

function promiseDelayedStartupFinished(win) {
  return new Promise(resolve => whenDelayedStartupFinished(win, resolve));
}
