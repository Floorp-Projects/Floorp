/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const TEST_URI = TEST_PATH + "file_cross_process_csp_inheritance.html";
const DATA_URI =
  "data:text/html,<html>test-same-diff-process-csp-inhertiance</html>";

const FISSION_ENABLED = SpecialPowers.useRemoteSubframes;

function getCurrentPID(aBrowser) {
  return SpecialPowers.spawn(aBrowser, [], () => {
    return Services.appinfo.processID;
  });
}

function getCurrentURI(aBrowser) {
  return SpecialPowers.spawn(aBrowser, [], () => {
    let channel = content.docShell.currentDocumentChannel;
    return channel.URI.asciiSpec;
  });
}

function verifyResult(
  aTestName,
  aBrowser,
  aDataURI,
  aPID,
  aSamePID,
  aFissionEnabled
) {
  return SpecialPowers.spawn(
    aBrowser,
    [{ aTestName, aDataURI, aPID, aSamePID, aFissionEnabled }],
    async function({ aTestName, aDataURI, aPID, aSamePID, aFissionEnabled }) {
      // sanity, to make sure the correct URI was loaded
      let channel = content.docShell.currentDocumentChannel;
      is(
        channel.URI.asciiSpec,
        aDataURI,
        aTestName + ": correct data uri loaded"
      );

      // check that the process ID is the same/different when opening the new tab
      let pid = Services.appinfo.processID;
      if (aSamePID) {
        is(pid, aPID, aTestName + ": process ID needs to be identical");
      } else if (aFissionEnabled) {
        // TODO: Fission discards dom.noopener.newprocess.enabled and puts
        // data: URIs in the same process. Unfortunately todo_isnot is not
        // defined in that scope, hence we have to use a workaround.
        todo(
          false,
          pid == aPID,
          ": process ID needs to be different in fission"
        );
      } else {
        isnot(pid, aPID, aTestName + ": process ID needs to be different");
      }

      // finally, evaluate that the CSP was set.
      let cspOBJ = JSON.parse(content.document.cspJSON);
      let policies = cspOBJ["csp-policies"];
      is(policies.length, 1, "should be one policy");
      let policy = policies[0];
      is(
        policy["script-src"],
        "'none'",
        aTestName + ": script-src directive matches"
      );
    }
  );
}

async function simulateCspInheritanceForNewTab(aTestName, aSamePID) {
  await BrowserTestUtils.withNewTab(TEST_URI, async function(browser) {
    // do some sanity checks
    let currentURI = await getCurrentURI(gBrowser.selectedBrowser);
    is(currentURI, TEST_URI, aTestName + ": correct test uri loaded");

    let pid = await getCurrentPID(gBrowser.selectedBrowser);
    let loadPromise = BrowserTestUtils.waitForNewTab(gBrowser, DATA_URI, true);
    // simulate click
    BrowserTestUtils.synthesizeMouseAtCenter(
      "#testLink",
      {},
      gBrowser.selectedBrowser
    );
    let tab = await loadPromise;
    gBrowser.selectTabAtIndex(2);
    await verifyResult(
      aTestName,
      gBrowser.selectedBrowser,
      DATA_URI,
      pid,
      aSamePID,
      FISSION_ENABLED
    );
    await BrowserTestUtils.removeTab(tab);
  });
}

add_task(async function test_csp_inheritance_diff_process() {
  // forcing the new data: URI load to happen in a *new* process by flipping the pref
  // to force <a rel="noopener" ...> to be loaded in a new process.
  await SpecialPowers.pushPrefEnv({
    set: [["dom.noopener.newprocess.enabled", true]],
  });
  await simulateCspInheritanceForNewTab("diff-process-inheritance", false);
});

add_task(async function test_csp_inheritance_same_process() {
  // forcing the new data: URI load to happen in a *same* process by resetting the pref
  // and loaded <a rel="noopener" ...> in the *same* process.
  await SpecialPowers.pushPrefEnv({
    set: [["dom.noopener.newprocess.enabled", false]],
  });
  await simulateCspInheritanceForNewTab("same-process-inheritance", true);
});
