/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PRELOAD_PREF = "browser.newtab.preload";

gDirectorySource = "data:application/json," + JSON.stringify({
  "directory": [{
    url: "http://example.com/organic",
    type: "organic"
  }, {
    url: "http://localhost/sponsored",
    type: "sponsored"
  }]
});

add_task(function* () {
  yield pushPrefs([PRELOAD_PREF, false]);

  let originalReportSitesAction  = DirectoryLinksProvider.reportSitesAction;
  registerCleanupFunction(() => {
    DirectoryLinksProvider.reportSitesAction = originalReportSitesAction;
  });

  let expected = {};

  function expectReportSitesAction() {
    return new Promise(resolve => {
      DirectoryLinksProvider.reportSitesAction = function(sites, action, siteIndex) {
        let {link} = sites[siteIndex];
        is(link.type, expected.type, "got expected type");
        is(action, expected.action, "got expected action");
        is(NewTabUtils.pinnedLinks.isPinned(link), expected.pinned, "got expected pinned");
        resolve();
      }
    });
  }

  // Test that the last visible site (index 1) is reported
  let reportSitesPromise = expectReportSitesAction();
  expected.type = "sponsored";
  expected.action = "view";
  expected.pinned = false;
  yield* addNewTabPageTab();
  yield reportSitesPromise;

  // Click the pin button on the link in the 1th tile spot
  expected.action = "pin";
  // tiles become "history" when pinned
  expected.type = "history";
  expected.pinned = true;
  let pagesUpdatedPromise = whenPagesUpdated();
  reportSitesPromise = expectReportSitesAction();

  yield BrowserTestUtils.synthesizeMouseAtCenter(".newtab-cell + .newtab-cell .newtab-control-pin", {}, gBrowser.selectedBrowser);
  yield pagesUpdatedPromise;
  yield reportSitesPromise;

  // Unpin that link
  expected.action = "unpin";
  expected.pinned = false;
  pagesUpdatedPromise = whenPagesUpdated();
  reportSitesPromise = expectReportSitesAction();
  yield BrowserTestUtils.synthesizeMouseAtCenter(".newtab-cell + .newtab-cell .newtab-control-pin", {}, gBrowser.selectedBrowser);
  yield pagesUpdatedPromise;
  yield reportSitesPromise;

  // Block the site in the 0th tile spot
  expected.type = "organic";
  expected.action = "block";
  expected.pinned = false;
  pagesUpdatedPromise = whenPagesUpdated();
  reportSitesPromise = expectReportSitesAction();
  yield BrowserTestUtils.synthesizeMouseAtCenter(".newtab-site .newtab-control-block", {}, gBrowser.selectedBrowser);
  yield pagesUpdatedPromise;
  yield reportSitesPromise;

  // Click the 1th link now in the 0th tile spot
  expected.type = "history";
  expected.action = "click";
  reportSitesPromise = expectReportSitesAction();
  yield BrowserTestUtils.synthesizeMouseAtCenter(".newtab-site", {}, gBrowser.selectedBrowser);
  yield reportSitesPromise;
});
