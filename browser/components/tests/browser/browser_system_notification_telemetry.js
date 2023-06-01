/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function handleCommandLine(args, state) {
  let newWinPromise;
  let target = Services.urlFormatter.formatURLPref(
    "browser.shell.defaultBrowserAgent.thanksURL"
  );

  const EXISTING_FILE = Cc["@mozilla.org/file/local;1"].createInstance(
    Ci.nsIFile
  );
  EXISTING_FILE.initWithPath(getTestFilePath("dummy.pdf"));

  if (state == Ci.nsICommandLine.STATE_INITIAL_LAUNCH) {
    newWinPromise = BrowserTestUtils.waitForNewWindow({
      url: target, // N.b.: trailing slashes matter when matching.
    });
  }

  let cmdLineHandler = Cc["@mozilla.org/browser/final-clh;1"].getService(
    Ci.nsICommandLineHandler
  );

  let fakeCmdLine = Cu.createCommandLine(args, EXISTING_FILE.parent, state);
  cmdLineHandler.handle(fakeCmdLine);

  if (newWinPromise) {
    let newWin = await newWinPromise;
    await BrowserTestUtils.closeWindow(newWin);
  } else {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
}

// Launching from the WDBA should open the "thanks" page and should send a
// telemetry event.
add_task(async function test_launched_to_handle_default_browser_agent() {
  await handleCommandLine(
    ["-to-handle-default-browser-agent"],
    Ci.nsICommandLine.STATE_INITIAL_LAUNCH
  );

  TelemetryTestUtils.assertEvents(
    [{ extra: { name: "default-browser-agent" } }],
    {
      category: "browser.launched_to_handle",
      method: "system_notification",
      object: "toast",
    }
  );
});
