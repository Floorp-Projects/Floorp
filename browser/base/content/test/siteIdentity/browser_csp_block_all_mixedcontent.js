/*
 * Description of the Test:
 * We load an https page which uses a CSP including block-all-mixed-content.
 * The page tries to load a script over http. We make sure the UI is not
 * influenced when blocking the mixed content. In particular the page
 * should still appear fully encrypted with a green lock.
 */

const PRE_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
var gTestBrowser = null;

// ------------------------------------------------------
function cleanUpAfterTests() {
  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}

// ------------------------------------------------------
async function verifyUInotDegraded() {
  // make sure that not mixed content is loaded and also not blocked
  await assertMixedContentBlockingState(gTestBrowser, {
    activeLoaded: false,
    activeBlocked: false,
    passiveLoaded: false,
  });
  // clean up and finish test
  cleanUpAfterTests();
}

// ------------------------------------------------------
function runTests() {
  var newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  newTab.linkedBrowser.stop();

  // Starting the test
  var url = PRE_PATH + "file_csp_block_all_mixedcontent.html";
  BrowserTestUtils.browserLoaded(gTestBrowser, false, url).then(
    verifyUInotDegraded
  );
  BrowserTestUtils.startLoadingURIString(gTestBrowser, url);
}

// ------------------------------------------------------
function test() {
  // Performing async calls, e.g. 'onload', we have to wait till all of them finished
  waitForExplicitFinish();

  SpecialPowers.pushPrefEnv(
    { set: [["security.mixed_content.block_active_content", true]] },
    function () {
      runTests();
    }
  );
}
