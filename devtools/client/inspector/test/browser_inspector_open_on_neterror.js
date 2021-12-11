/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test that inspector works correctly when opened against a net error page

const TEST_URL_1 = "http://127.0.0.1:36325/";
const TEST_URL_2 = "data:text/html,<html><body>test-doc-2</body></html>";

add_task(async function() {
  // We cannot directly use addTab here as waiting for error pages requires
  // a specific code path.
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, TEST_URL_1);
  await BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);

  const { inspector } = await openInspector();
  ok(true, "Inspector loaded on the already opened net error");

  const documentURI = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => content.document.documentURI
  );
  ok(
    documentURI.startsWith("about:neterror"),
    "content is really a net error page."
  );

  const netErrorNode = await getNodeFront("#errorPageContainer", inspector);
  ok(netErrorNode, "The inspector can get a node front from the neterror page");

  info("Navigate to a valid url");
  await navigateTo(TEST_URL_2);

  is(
    await getDisplayedNodeTextContent("body", inspector),
    "test-doc-2",
    "Inspector really inspects the valid url"
  );
});
