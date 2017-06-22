"use strict";

/**
 * Test for Bug 1359204
 *
 * Loading a local file, then view-source on that file. Make sure that
 * clicking a link within that view-source page is not blocked by security checks.
 */

add_task(async function test_click_link_within_view_source() {
  let TEST_FILE = "file_click_link_within_view_source.html";
  let TEST_FILE_URI = getChromeDir(getResolvedURI(gTestPath));
  TEST_FILE_URI.append(TEST_FILE);
  TEST_FILE_URI = Services.io.newFileURI(TEST_FILE_URI).spec;

  let DUMMY_FILE = "dummy_page.html";
  let DUMMY_FILE_URI = getChromeDir(getResolvedURI(gTestPath));
  DUMMY_FILE_URI.append(DUMMY_FILE);
  DUMMY_FILE_URI = Services.io.newFileURI(DUMMY_FILE_URI).spec;

  await BrowserTestUtils.withNewTab(TEST_FILE_URI, async function(aBrowser) {
    let tabSpec = gBrowser.selectedBrowser.currentURI.spec;
    info("loading: " + tabSpec);
    ok(tabSpec.startsWith("file://") && tabSpec.endsWith(TEST_FILE),
       "sanity check to make sure html loaded");

    info("click view-source of html");
    let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
    document.getElementById("View:PageSource").doCommand();

    let tab = await tabPromise;
    tabSpec = gBrowser.selectedBrowser.currentURI.spec;
    info("loading: " + tabSpec);
    ok(tabSpec.startsWith("view-source:file://") && tabSpec.endsWith(TEST_FILE),
       "loading view-source of html succeeded");

    info("click testlink within view-source page");
    let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, url => url.endsWith("dummy_page.html"));
    await ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
      if (content.document.readyState != "complete") {
        await ContentTaskUtils.waitForEvent(content.document, "readystatechange", false, () =>
          content.document.readyState == "complete");
      }
      // document.getElementById() does not work on a view-source page, hence we use document.links
      let linksOnPage = content.document.links;
      is (linksOnPage.length, 1, "sanity check: make sure only one link is present on page");
      let myLink = content.document.links[0];
      myLink.click();
    });

    await loadPromise;

    tabSpec = gBrowser.selectedBrowser.currentURI.spec;
    info("loading: " + tabSpec);
    ok(tabSpec.startsWith("view-source:file://") && tabSpec.endsWith(DUMMY_FILE),
       "loading view-source of html succeeded");

    await BrowserTestUtils.removeTab(tab);
  });
});
