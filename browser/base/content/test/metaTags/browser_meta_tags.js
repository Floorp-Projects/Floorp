/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals gBrowser */
/* This tests that with the page meta_tags.html, ContentMetaHandler.jsm parses out
 * the meta tags avilable and only stores the best one for description and one for
 * preview image url. In the case of this test, the best defined meta tags are
 * "og:description" and "og:image:url". The list of meta tags and their order of
 * preference is found in ContentMetaHandler.jsm. Because there is debounce logic
 * in ContentLinkHandler.jsm to only make one single SQL update, we have to wait
 * for some time before checking that the page info was stored correctly.
 */
add_task(async function test() {
    Components.utils.import("resource://gre/modules/PlacesUtils.jsm");
    const URL = "https://example.com/browser/browser/base/content/test/metaTags/meta_tags.html";
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

    // Wait until places has stored the page info
    let pageInfo;
    await BrowserTestUtils.waitForCondition(async () => {
      pageInfo = await PlacesUtils.history.fetch(URL, {"includeMeta": true});
      const {previewImageURL, description} = pageInfo;
      return previewImageURL.href && description;
    });
    is(pageInfo.description, "og:description", "got the correct description");
    is(pageInfo.previewImageURL.href, "og:image:url", "got the correct preview image");
    await BrowserTestUtils.removeTab(tab);
});

