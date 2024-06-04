/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const TEST_URL = TEST_ROOT + "204.sjs";
const BLANK_URL = TEST_ROOT + "blank.html";

// Test for bug 1626362.
add_task(async function () {
  await BrowserTestUtils.withNewTab("about:robots", async function (aBrowser) {
    // Get the current pid for browser for comparison later, we expect this
    // to be the parent process for about:robots.
    let browserPid = await SpecialPowers.spawn(aBrowser, [], () => {
      return Services.appinfo.processID;
    });

    is(
      Services.appinfo.processID,
      browserPid,
      "about:robots should have loaded in the parent"
    );

    // Attempt to load a uri that returns a 204 response, and then check that
    // we didn't process switch for it.
    let stopped = BrowserTestUtils.browserStopped(aBrowser, TEST_URL, true);
    BrowserTestUtils.startLoadingURIString(aBrowser, TEST_URL);
    await stopped;

    let newPid = await SpecialPowers.spawn(aBrowser, [], () => {
      return Services.appinfo.processID;
    });

    is(
      browserPid,
      newPid,
      "Shouldn't change process when we get a 204 response"
    );

    // Load a valid http page and confirm that we did change process
    // to confirm that we weren't in a web process to begin with.
    let loaded = BrowserTestUtils.browserLoaded(aBrowser, false, BLANK_URL);
    BrowserTestUtils.startLoadingURIString(aBrowser, BLANK_URL);
    await loaded;

    newPid = await SpecialPowers.spawn(aBrowser, [], () => {
      return Services.appinfo.processID;
    });

    isnot(browserPid, newPid, "Should change process for a valid response");
  });
});
