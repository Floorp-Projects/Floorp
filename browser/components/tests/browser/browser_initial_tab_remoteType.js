/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests test that the initial browser tab has the right
 * process type assigned to it on creation, which avoids needless
 * process flips.
 */

"use strict";

const PRIVILEGEDABOUT_PROCESS_PREF =
  "browser.tabs.remote.separatePrivilegedContentProcess";
const PRIVILEGEDABOUT_PROCESS_ENABLED = Services.prefs.getBoolPref(
  PRIVILEGEDABOUT_PROCESS_PREF
);

const REMOTE_BROWSER_SHOWN = "remote-browser-shown";

// When the privileged content process is enabled, we expect about:home
// to load in it. Otherwise, it's in a normal web content process.
const EXPECTED_ABOUTHOME_REMOTE_TYPE = PRIVILEGEDABOUT_PROCESS_ENABLED
  ? E10SUtils.PRIVILEGEDABOUT_REMOTE_TYPE
  : E10SUtils.DEFAULT_REMOTE_TYPE;

/**
 * Test helper function that takes an nsICommandLine, and passes it
 * into the default command line handler for the browser. It expects
 * a new browser window to open, and then checks that the expected page
 * loads in the initial tab in the expected remote type, without doing
 * unnecessary process flips. The helper function then closes the window.
 *
 * @param aCmdLine (nsICommandLine)
 *        The command line to be processed by the default
 *        nsICommandLineHandler
 * @param aExpectedURL (string)
 *        The URL that the initial browser tab is expected to load.
 * @param aRemoteType (string)
 *        The expected remoteType on the initial browser tab.
 * @returns Promise
 *        Resolves once the checks have completed, and the opened window
 *        have been closed.
 */
async function assertOneRemoteBrowserShown(
  aCmdLine,
  aExpectedURL,
  aRemoteType
) {
  let shownRemoteBrowsers = 0;
  let observer = () => {
    shownRemoteBrowsers++;
  };
  Services.obs.addObserver(observer, REMOTE_BROWSER_SHOWN);

  let newWinPromise = BrowserTestUtils.waitForNewWindow({
    url: aExpectedURL,
  });

  let cmdLineHandler = Cc["@mozilla.org/browser/final-clh;1"].getService(
    Ci.nsICommandLineHandler
  );
  cmdLineHandler.handle(aCmdLine);

  let newWin = await newWinPromise;

  Services.obs.removeObserver(observer, REMOTE_BROWSER_SHOWN);

  if (aRemoteType == E10SUtils.WEB_REMOTE_TYPE) {
    Assert.ok(
      E10SUtils.isWebRemoteType(newWin.gBrowser.selectedBrowser.remoteType)
    );
  } else {
    Assert.equal(newWin.gBrowser.selectedBrowser.remoteType, aRemoteType);
  }

  Assert.equal(
    shownRemoteBrowsers,
    1,
    "Should have only shown 1 remote browser"
  );
  await BrowserTestUtils.closeWindow(newWin);
}

/**
 * Constructs an object that implements an nsICommandLine that should
 * cause the default nsICommandLineHandler to open aURL as the initial
 * tab in a new window. The returns nsICommandLine is stateful, and
 * shouldn't be reused.
 *
 * @param aURL (string)
 *        The URL to load in the initial tab of the new window.
 * @returns nsICommandLine
 */
function constructOnePageCmdLine(aURL) {
  return Cu.createCommandLine(
    ["-url", aURL],
    null,
    Ci.nsICommandLine.STATE_INITIAL_LAUNCH
  );
}

add_setup(async function () {
  NewTabPagePreloading.removePreloadedBrowser(window);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.newtab.preload", false],
      ["browser.startup.homepage", "about:home"],
      ["browser.startup.page", 1],
    ],
  });
});

/**
 * This tests the default case, where no arguments are passed.
 */
add_task(async function test_default_args_and_homescreen() {
  let cmdLine = Cu.createCommandLine(
    [],
    null,
    Ci.nsICommandLine.STATE_INITIAL_LAUNCH
  );
  await assertOneRemoteBrowserShown(
    cmdLine,
    "about:home",
    EXPECTED_ABOUTHOME_REMOTE_TYPE
  );
});

/**
 * This tests the case where about:home is passed as the lone
 * argument.
 */
add_task(async function test_abouthome_arg() {
  const URI = "about:home";
  let cmdLine = constructOnePageCmdLine(URI);
  await assertOneRemoteBrowserShown(
    cmdLine,
    URI,
    EXPECTED_ABOUTHOME_REMOTE_TYPE
  );
});

/**
 * This tests the case where example.com is passed as the lone
 * argument.
 */
add_task(async function test_examplecom_arg() {
  const URI = "http://example.com/";
  let cmdLine = constructOnePageCmdLine(URI);
  await assertOneRemoteBrowserShown(
    cmdLine,
    URI,
    E10SUtils.DEFAULT_REMOTE_TYPE
  );
});
