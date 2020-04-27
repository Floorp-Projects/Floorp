"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const HTML_URI = TEST_PATH + "file_data_load_inherit_csp.html";
const DATA_URI = "data:text/html;html,<html><body>foo</body></html>";

function setDataHrefOnLink(aBrowser, aDataURI) {
  return SpecialPowers.spawn(aBrowser, [aDataURI], function(uri) {
    let link = content.document.getElementById("testlink");
    link.href = uri;
  });
}

function verifyCSP(aTestName, aBrowser, aDataURI) {
  return SpecialPowers.spawn(
    aBrowser,
    [{ aTestName, aDataURI }],
    async function({ aTestName, aDataURI }) {
      let channel = content.docShell.currentDocumentChannel;
      is(channel.URI.spec, aDataURI, "testing CSP for " + aTestName);
      let cspJSON = content.document.cspJSON;
      let cspOBJ = JSON.parse(cspJSON);
      let policies = cspOBJ["csp-policies"];
      is(policies.length, 1, "should be one policy");
      let policy = policies[0];
      is(
        policy["script-src"],
        "'unsafe-inline'",
        "script-src directive matches"
      );
    }
  );
}

add_task(async function setup() {
  // allow top level data: URI navigations, otherwise clicking data: link fails
  await SpecialPowers.pushPrefEnv({
    set: [["security.data_uri.block_toplevel_data_uri_navigations", false]],
  });
});

add_task(async function test_data_csp_inheritance_regular_click() {
  await BrowserTestUtils.withNewTab(HTML_URI, async function(browser) {
    let loadPromise = BrowserTestUtils.browserLoaded(browser, false, DATA_URI);
    // set the data href + simulate click
    await setDataHrefOnLink(gBrowser.selectedBrowser, DATA_URI);
    BrowserTestUtils.synthesizeMouseAtCenter(
      "#testlink",
      {},
      gBrowser.selectedBrowser
    );
    await loadPromise;
    await verifyCSP("click()", gBrowser.selectedBrowser, DATA_URI);
  });
});

add_task(async function test_data_csp_inheritance_ctrl_click() {
  await BrowserTestUtils.withNewTab(HTML_URI, async function(browser) {
    let loadPromise = BrowserTestUtils.waitForNewTab(gBrowser, DATA_URI, true);
    // set the data href + simulate ctrl+click
    await setDataHrefOnLink(gBrowser.selectedBrowser, DATA_URI);
    BrowserTestUtils.synthesizeMouseAtCenter(
      "#testlink",
      { ctrlKey: true, metaKey: true },
      gBrowser.selectedBrowser
    );
    let tab = await loadPromise;
    gBrowser.selectTabAtIndex(2);
    await verifyCSP("ctrl-click()", gBrowser.selectedBrowser, DATA_URI);
    await BrowserTestUtils.removeTab(tab);
  });
});

add_task(
  async function test_data_csp_inheritance_right_click_open_link_in_new_tab() {
    await BrowserTestUtils.withNewTab(HTML_URI, async function(browser) {
      let loadPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        DATA_URI,
        true
      );
      // set the data href + simulate right-click open link in tab
      await setDataHrefOnLink(gBrowser.selectedBrowser, DATA_URI);
      BrowserTestUtils.waitForEvent(document, "popupshown", false, event => {
        // These are operations that must be executed synchronously with the event.
        document.getElementById("context-openlinkintab").doCommand();
        event.target.hidePopup();
        return true;
      });
      BrowserTestUtils.synthesizeMouseAtCenter(
        "#testlink",
        { type: "contextmenu", button: 2 },
        gBrowser.selectedBrowser
      );

      let tab = await loadPromise;
      gBrowser.selectTabAtIndex(2);
      await verifyCSP(
        "right-click-open-in-new-tab()",
        gBrowser.selectedBrowser,
        DATA_URI
      );
      await BrowserTestUtils.removeTab(tab);
    });
  }
);
