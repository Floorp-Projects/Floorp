const TEST_PAGE =
  "http://example.org/browser/browser/base/content/test/general/dummy_page.html";
const TEST_REFERRER = "http://mochi.test:8888/";

const ReferrerInfo = Components.Constructor(
  "@mozilla.org/referrer-info;1",
  "nsIReferrerInfo",
  "init"
);

const referrerInfo = new ReferrerInfo(
  Ci.nsIReferrerInfo.ORIGIN,
  true,
  Services.io.newURI(TEST_REFERRER)
);

function queryEntry(browser) {
  function queryEntryScript() {
    let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    let sessionHistory = webNav.sessionHistory;
    let entry = sessionHistory.legacySHistory.getEntryAtIndex(
      sessionHistory.count - 1
    );
    let result = {
      uri: entry.URI.spec,
      referrerInfo: E10SUtils.serializeReferrerInfo(entry.referrerInfo),
    };
    sendAsyncMessage("Test:queryEntry", result);
  }

  return new Promise(resolve => {
    browser.messageManager.addMessageListener(
      "Test:queryEntry",
      function listener({ data }) {
        browser.messageManager.removeMessageListener(
          "Test:queryEntry",
          listener
        );
        resolve(data);
      }
    );

    browser.messageManager.loadFrameScript(
      "data:,(" + queryEntryScript.toString() + ")();",
      true
    );
  });
}

var checkResult = async function(isRemote, browserKey, uri) {
  is(
    gBrowser.selectedBrowser.isRemoteBrowser,
    isRemote,
    "isRemoteBrowser should be correct"
  );

  is(
    gBrowser.selectedBrowser.permanentKey,
    browserKey,
    "browser.permanentKey should be correct"
  );

  let entry = await queryEntry(gBrowser.selectedBrowser);

  is(entry.uri, uri, "Uri should be correct");

  // Main process like about:mozilla does not trigger the real network request.
  // So we don't store referrerInfo in sessionHistory in that case.
  // Besides, the referrerInfo stored in sessionHistory was computed, we only
  // check pre-computed things.
  if (isRemote) {
    let resultReferrerInfo = E10SUtils.deserializeReferrerInfo(
      entry.referrerInfo
    );
    is(
      resultReferrerInfo.originalReferrer.spec,
      referrerInfo.originalReferrer.spec,
      "originalReferrer should be correct"
    );
    is(
      resultReferrerInfo.sendReferrer,
      referrerInfo.sendReferrer,
      "sendReferrer should be correct"
    );
    is(
      resultReferrerInfo.referrerPolicy,
      referrerInfo.referrerPolicy,
      "referrerPolicy should be correct"
    );
  } else {
    is(entry.referrerInfo, null, "ReferrerInfo should be correct");
  }
};

var waitForLoad = async function(uri) {
  info("waitForLoad " + uri);
  let loadURIOptions = {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    referrerInfo,
  };
  gBrowser.selectedBrowser.webNavigation.loadURI(uri, loadURIOptions);

  await BrowserTestUtils.browserStopped(gBrowser);
};

// Tests referrerInfo when navigating from a page in the remote process to main
// process and vice versa.
// The changing process code flow is (cpp) docshell.shouldLoadURI
// -> (JS) browser.shouldLoadURI -> E10sUtils.redirectLoad
// -> ContentRestore.restoreTabContent.
// Finally, docshell will do the load in correct process with the input
// referrerInfo and store an entry to SessionHistory
add_task(async function test_navigation() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let testURI = TEST_PAGE;
  let { permanentKey } = gBrowser.selectedBrowser;
  // Load a remote page
  await waitForLoad(testURI);
  await checkResult(true, permanentKey, testURI);

  // Load a non-remote page
  testURI = "about:mozilla";
  await waitForLoad(testURI);
  await checkResult(false, permanentKey, testURI);

  // Load a remote page
  testURI = TEST_PAGE;
  await waitForLoad(testURI);
  await checkResult(true, permanentKey, testURI);

  gBrowser.removeCurrentTab();
});
