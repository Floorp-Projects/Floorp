/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

//
// Try to open a tab.  This provides code coverage for a few things,
// although currently there's no automated functional test of correctness:
//
// * On opt builds, when the tab is closed and the process exits, it
//   will hang for 3s and the parent will kill it after 2s.
//
// * On debug[*] builds, the parent process will wait until the
//   process exits normally; but also, on browser shutdown, the
//   preallocated content processes will block parent shutdown in
//   WillDestroyCurrentMessageLoop.
//
// [*] Also sanitizer and code coverage builds.
//

add_task(async function () {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "https://example.com/",
      forceNewProcess: true,
    },
    async function (browser) {
      // browser.frameLoader.remoteTab.osPid is the child pid; once we
      // have a way to get notifications about child process termination
      // events, that could be useful.
      ok(true, "Browser isn't broken");
    }
  );
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 4000));
  ok(true, "Still running after child process (hopefully) exited");
});
