/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test link helpers openDocLink, openTrustedLink.

// Use any valid test page here.
const TEST_URI = TEST_URI_ROOT_SSL + "dummy.html";

const { openDocLink, openTrustedLink } = require("devtools/client/shared/link");

add_task(async function() {
  // Open a link to a page that will not trigger any request.
  info("Open web link to example.com test page");
  openDocLink(TEST_URI);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  is(
    gBrowser.selectedBrowser.currentURI.spec,
    TEST_URI,
    "openDocLink opened a tab with the expected url"
  );

  info("Open trusted link to about:config");
  openTrustedLink("about:config");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  is(
    gBrowser.selectedBrowser.currentURI.spec,
    "about:config",
    "openTrustedLink opened a tab with the expected url"
  );

  await removeTab(gBrowser.selectedTab);
  await removeTab(gBrowser.selectedTab);
});
