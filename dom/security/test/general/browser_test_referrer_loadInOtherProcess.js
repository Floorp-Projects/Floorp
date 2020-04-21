const TEST_PAGE =
  "http://example.org/browser/browser/base/content/test/general/dummy_page.html";
const TEST_REFERRER = "http://mochi.test:8888/";

const ReferrerInfo = Components.Constructor(
  "@mozilla.org/referrer-info;1",
  "nsIReferrerInfo",
  "init"
);

let referrerInfo = new ReferrerInfo(
  Ci.nsIReferrerInfo.ORIGIN,
  true,
  Services.io.newURI(TEST_REFERRER)
);
let deReferrerInfo = E10SUtils.serializeReferrerInfo(referrerInfo);

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

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [{ uri, referrerInfo: deReferrerInfo, isRemote }],
    async function(args) {
      let webNav = content.docShell.QueryInterface(Ci.nsIWebNavigation);
      let sessionHistory = webNav.sessionHistory;
      let entry = sessionHistory.legacySHistory.getEntryAtIndex(
        sessionHistory.count - 1
      );

      var { E10SUtils } = SpecialPowers.Cu.import(
        "resource://gre/modules/E10SUtils.jsm"
      );

      Assert.equal(entry.URI.spec, args.uri, "Uri should be correct");

      // Main process like about:mozilla does not trigger the real network request.
      // So we don't store referrerInfo in sessionHistory in that case.
      // Besides, the referrerInfo stored in sessionHistory was computed, we only
      // check pre-computed things.
      if (args.isRemote) {
        let resultReferrerInfo = entry.referrerInfo;
        let expectedReferrerInfo = E10SUtils.deserializeReferrerInfo(
          args.referrerInfo
        );

        Assert.equal(
          resultReferrerInfo.originalReferrer.spec,
          expectedReferrerInfo.originalReferrer.spec,
          "originalReferrer should be correct"
        );
        Assert.equal(
          resultReferrerInfo.sendReferrer,
          expectedReferrerInfo.sendReferrer,
          "sendReferrer should be correct"
        );
        Assert.equal(
          resultReferrerInfo.referrerPolicy,
          expectedReferrerInfo.referrerPolicy,
          "referrerPolicy should be correct"
        );
      } else {
        Assert.equal(
          entry.referrerInfo,
          null,
          "ReferrerInfo should be correct"
        );
      }
    }
  );
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
  // Navigate from non remote to remote
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let testURI = TEST_PAGE;
  let { permanentKey } = gBrowser.selectedBrowser;
  await waitForLoad(testURI);
  await checkResult(true, permanentKey, testURI);
  gBrowser.removeCurrentTab();

  // Navigate from remote to non-remote
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, TEST_PAGE);
  testURI = "about:mozilla";
  permanentKey = gBrowser.selectedBrowser.permanentKey;
  await waitForLoad(testURI);
  await checkResult(false, permanentKey, testURI);

  gBrowser.removeCurrentTab();
});
