/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test Summary:
// 1.  call ss.setWindowState with a broken state
// 1a. ensure that it doesn't throw.

function test() {
  waitForExplicitFinish();

  let brokenState = {
    windows: [
      { tabs: [{ entries: [{ url: "about:mozilla" }] }] }
      //{ tabs: [{ entries: [{ url: "about:robots" }] }] },
    ],
    selectedWindow: 2
  };
  let brokenStateString = JSON.stringify(brokenState);

  let gotError = false;
  try {
    ss.setWindowState(window, brokenStateString, true);
  }
  catch (ex) {
    gotError = true;
    info(ex);
  }

  ok(!gotError, "ss.setWindowState did not throw an error");

  // Make sure that we reset the state. Use a full state just in case things get crazy.
  let blankState = { windows: [{ tabs: [{ entries: [{ url: "about:blank" }] }]}]};
  waitForBrowserState(blankState, finish);
}

