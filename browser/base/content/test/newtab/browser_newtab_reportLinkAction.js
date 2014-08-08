/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PRELOAD_PREF = "browser.newtab.preload";

gDirectorySource = "data:application/json," + JSON.stringify({
  "en-US": [{
    url: "http://example.com/organic",
    type: "organic"
  }, {
    url: "http://localhost/sponsored",
    type: "sponsored"
  }]
});

function runTests() {
  Services.prefs.setBoolPref(PRELOAD_PREF, false);

  let originalReportSitesAction  = DirectoryLinksProvider.reportSitesAction;
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(PRELOAD_PREF);
    DirectoryLinksProvider.reportSitesAction = originalReportSitesAction;
  });

  let expected = {};
  DirectoryLinksProvider.reportSitesAction = function(sites, action, siteIndex) {
    let {link} = sites[siteIndex];
    is(link.type, expected.type, "got expected type");
    is(action, expected.action, "got expected action");
    is(NewTabUtils.pinnedLinks.isPinned(link), expected.pinned, "got expected pinned");
    executeSoon(TestRunner.next);
  }

  // Test that the last visible site (index 1) is reported
  expected.type = "sponsored";
  expected.action = "view";
  expected.pinned = false;
  yield addNewTabPageTab();
  yield null; // wait for reportSitesAction

  // Click the pin button on the link in the 1th tile spot
  let siteNode = getCell(1).node.querySelector(".newtab-site");
  let pinButton = siteNode.querySelector(".newtab-control-pin");
  expected.action = "pin";
  expected.pinned = true;
  yield EventUtils.synthesizeMouseAtCenter(pinButton, {}, getContentWindow());

  // Unpin that link
  expected.action = "unpin";
  expected.pinned = false;
  yield EventUtils.synthesizeMouseAtCenter(pinButton, {}, getContentWindow());
  yield whenPagesUpdated();

  // Block the site in the 0th tile spot
  let blockedSite = getCell(0).node.querySelector(".newtab-site");
  let blockButton = blockedSite.querySelector(".newtab-control-block");
  expected.type = "organic";
  expected.action = "block";
  expected.pinned = false;
  yield EventUtils.synthesizeMouseAtCenter(blockButton, {}, getContentWindow());
  yield whenPagesUpdated();

  // Click the 1th link now in the 0th tile spot
  expected.type = "sponsored";
  expected.action = "click";
  yield EventUtils.synthesizeMouseAtCenter(siteNode, {}, getContentWindow());
}
