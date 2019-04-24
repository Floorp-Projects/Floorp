/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");
const TEST_URI = TEST_PATH + "file_cross_process_csp_inheritance.html";
const DATA_URI = "data:text/html,<html>test-same-diff-process-csp-inhertiance</html>";

function getCurrentPID(aBrowser) {
  return ContentTask.spawn(aBrowser, null, () => {
    return Services.appinfo.processID;
  });
}

function getCurrentURI(aBrowser) {
  return ContentTask.spawn(aBrowser, null, () => {
    let channel = content.docShell.currentDocumentChannel;
    return channel.URI.asciiSpec;
  });
}

function verifyResult(aTestName, aBrowser, aDataURI, aPID, aSamePID) {
  return ContentTask.spawn(aBrowser, {aTestName, aDataURI, aPID, aSamePID}, async function({aTestName, aDataURI, aPID, aSamePID}) {
    // sanity, to make sure the correct URI was loaded
    let channel = content.docShell.currentDocumentChannel;
    is(channel.URI.asciiSpec, aDataURI, aTestName + ": correct data uri loaded");

    // check that the process ID is the same/different when opening the new tab
    let pid = Services.appinfo.processID;
    if (aSamePID) {
      is(pid, aPID, aTestName + ": process ID needs to be identical");
    } else {
      isnot(pid, aPID, aTestName + ": process ID needs to be different");
    }

    // finally, evaluate that the CSP was set.
    let principal = channel.loadInfo.triggeringPrincipal;
    let cspOBJ = JSON.parse(principal.cspJSON);
    let policies = cspOBJ["csp-policies"];
    is(policies.length, 1, "should be one policy");
    let policy = policies[0];
    is(policy["script-src"], "'none'", aTestName + ": script-src directive matches");
  });
}

async function simulateCspInheritanceForNewTab(aTestName, aSamePID) {
  await BrowserTestUtils.withNewTab(TEST_URI, async function(browser) {
    // do some sanity checks
    let currentURI = await getCurrentURI(gBrowser.selectedBrowser);
    is(currentURI, TEST_URI, aTestName + ": correct test uri loaded");

    let pid = await getCurrentPID(gBrowser.selectedBrowser);
    let loadPromise = BrowserTestUtils.waitForNewTab(gBrowser, DATA_URI);
    // simulate click
    BrowserTestUtils.synthesizeMouseAtCenter("#testLink", {},
                                             gBrowser.selectedBrowser);
    let tab = await loadPromise;
    gBrowser.selectTabAtIndex(2);
    await verifyResult(aTestName, gBrowser.selectedBrowser, DATA_URI, pid, aSamePID);
    await BrowserTestUtils.removeTab(tab);
  });
}

add_task(async function test_csp_inheritance_diff_process() {
  // forcing the new data: URI load to happen in a *new* process by flipping the pref
  // to force <a rel="noopener" ...> to be loaded in a new process.
  await SpecialPowers.pushPrefEnv({"set": [["dom.noopener.newprocess.enabled", true]]});
  await simulateCspInheritanceForNewTab("diff-process-inheritance", false);
});

add_task(async function test_csp_inheritance_same_process() {
  // forcing the new data: URI load to happen in a *same* process by resetting the pref
  // and loaded <a rel="noopener" ...> in the *same* process.
  await SpecialPowers.pushPrefEnv({"set": [["dom.noopener.newprocess.enabled", false]]});
  await simulateCspInheritanceForNewTab("same-process-inheritance", true);
});
