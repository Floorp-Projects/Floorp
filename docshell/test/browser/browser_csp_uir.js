"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");
const TEST_URI = TEST_PATH + "file_csp_uir.html"; // important to be http: to test upgrade-insecure-requests
const RESULT_URI = TEST_PATH.replace("http://", "https://") + "file_csp_uir_dummy.html";

function verifyCSP(aTestName, aBrowser, aResultURI) {
  return ContentTask.spawn(aBrowser, {aTestName, aResultURI}, async function({aTestName, aResultURI}) {
    let channel = content.docShell.currentDocumentChannel;
    is(channel.URI.asciiSpec, aResultURI, "testing CSP for " + aTestName);
  });
}

add_task(async function test_csp_inheritance_regular_click() {
  await BrowserTestUtils.withNewTab(TEST_URI, async function(browser) {
    let loadPromise = BrowserTestUtils.browserLoaded(browser, false, RESULT_URI);
    // set the data href + simulate click
    BrowserTestUtils.synthesizeMouseAtCenter("#testlink", {},
                                             gBrowser.selectedBrowser);
    await loadPromise;
    await verifyCSP("click()", gBrowser.selectedBrowser, RESULT_URI);
  });
});

add_task(async function test_csp_inheritance_ctrl_click() {
  await BrowserTestUtils.withNewTab(TEST_URI, async function(browser) {
    let loadPromise = BrowserTestUtils.waitForNewTab(gBrowser, RESULT_URI);
    // set the data href + simulate ctrl+click
    BrowserTestUtils.synthesizeMouseAtCenter("#testlink",
                                             { ctrlKey: true, metaKey: true },
                                             gBrowser.selectedBrowser);
    let tab = await loadPromise;
    gBrowser.selectTabAtIndex(2);
    await verifyCSP("ctrl-click()", gBrowser.selectedBrowser, RESULT_URI);
    await BrowserTestUtils.removeTab(tab);
  });
});

add_task(async function test_csp_inheritance_right_click_open_link_in_new_tab() {
  await BrowserTestUtils.withNewTab(TEST_URI, async function(browser) {
    let loadPromise = BrowserTestUtils.waitForNewTab(gBrowser, RESULT_URI);
    // set the data href + simulate right-click open link in tab
    BrowserTestUtils.waitForEvent(document, "popupshown", false, event => {
      // These are operations that must be executed synchronously with the event.
      document.getElementById("context-openlinkintab").doCommand();
      event.target.hidePopup();
      return true;
    });
    BrowserTestUtils.synthesizeMouseAtCenter("#testlink",
                                             { type: "contextmenu", button: 2 },
                                             gBrowser.selectedBrowser);

    let tab = await loadPromise;
    gBrowser.selectTabAtIndex(2);
    await verifyCSP("right-click-open-in-new-tab()", gBrowser.selectedBrowser, RESULT_URI);
    await BrowserTestUtils.removeTab(tab);
  });
});
