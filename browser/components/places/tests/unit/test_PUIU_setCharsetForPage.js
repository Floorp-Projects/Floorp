/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PrivateBrowsingUtils.sys.mjs"
);

const UTF8 = "UTF-8";
const UTF16 = "UTF-16";

const TEST_URI = "http://foo.com";
const TEST_BOOKMARKED_URI = "http://bar.com";

add_task(function setup() {
  let savedIsWindowPrivateFunc = PrivateBrowsingUtils.isWindowPrivate;
  PrivateBrowsingUtils.isWindowPrivate = () => false;

  registerCleanupFunction(() => {
    PrivateBrowsingUtils.isWindowPrivate = savedIsWindowPrivateFunc;
  });
});

add_task(async function test_simple_add() {
  // add pages to history
  await PlacesTestUtils.addVisits(TEST_URI);
  await PlacesTestUtils.addVisits(TEST_BOOKMARKED_URI);

  // create bookmarks on TEST_BOOKMARKED_URI
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_BOOKMARKED_URI,
    title: TEST_BOOKMARKED_URI.spec,
  });
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: TEST_BOOKMARKED_URI,
    title: TEST_BOOKMARKED_URI.spec,
  });

  // set charset on not-bookmarked page
  await PlacesUIUtils.setCharsetForPage(TEST_URI, UTF16, {});
  // set charset on bookmarked page
  await PlacesUIUtils.setCharsetForPage(TEST_BOOKMARKED_URI, UTF16, {});

  let pageInfo = await PlacesUtils.history.fetch(TEST_URI, {
    includeAnnotations: true,
  });
  Assert.equal(
    pageInfo.annotations.get(PlacesUtils.CHARSET_ANNO),
    UTF16,
    "Should return correct charset for a not-bookmarked page"
  );

  pageInfo = await PlacesUtils.history.fetch(TEST_BOOKMARKED_URI, {
    includeAnnotations: true,
  });
  Assert.equal(
    pageInfo.annotations.get(PlacesUtils.CHARSET_ANNO),
    UTF16,
    "Should return correct charset for a bookmarked page"
  );

  await PlacesUtils.history.clear();

  pageInfo = await PlacesUtils.history.fetch(TEST_URI, {
    includeAnnotations: true,
  });
  Assert.ok(
    !pageInfo,
    "Should not return pageInfo for a page after history cleared"
  );

  pageInfo = await PlacesUtils.history.fetch(TEST_BOOKMARKED_URI, {
    includeAnnotations: true,
  });
  Assert.equal(
    pageInfo.annotations.get(PlacesUtils.CHARSET_ANNO),
    UTF16,
    "Charset should still be set for a bookmarked page after history clear"
  );

  await PlacesUIUtils.setCharsetForPage(TEST_BOOKMARKED_URI, "");
  pageInfo = await PlacesUtils.history.fetch(TEST_BOOKMARKED_URI, {
    includeAnnotations: true,
  });
  Assert.strictEqual(
    pageInfo.annotations.get(PlacesUtils.CHARSET_ANNO),
    undefined,
    "Should not have a charset after it has been removed from the page"
  );
});

add_task(async function test_utf8_clears_saved_anno() {
  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits(TEST_URI);

  // set charset on bookmarked page
  await PlacesUIUtils.setCharsetForPage(TEST_URI, UTF16, {});

  let pageInfo = await PlacesUtils.history.fetch(TEST_URI, {
    includeAnnotations: true,
  });
  Assert.equal(
    pageInfo.annotations.get(PlacesUtils.CHARSET_ANNO),
    UTF16,
    "Should return correct charset for a not-bookmarked page"
  );

  // Now set the bookmark to a UTF-8 charset.
  await PlacesUIUtils.setCharsetForPage(TEST_URI, UTF8, {});

  pageInfo = await PlacesUtils.history.fetch(TEST_URI, {
    includeAnnotations: true,
  });
  Assert.strictEqual(
    pageInfo.annotations.get(PlacesUtils.CHARSET_ANNO),
    undefined,
    "Should have removed the charset for a UTF-8 page."
  );
});

add_task(async function test_private_browsing_not_saved() {
  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits(TEST_URI);

  // set charset on bookmarked page, but pretend this is a private browsing window.
  PrivateBrowsingUtils.isWindowPrivate = () => true;
  await PlacesUIUtils.setCharsetForPage(TEST_URI, UTF16, {});

  let pageInfo = await PlacesUtils.history.fetch(TEST_URI, {
    includeAnnotations: true,
  });
  Assert.strictEqual(
    pageInfo.annotations.get(PlacesUtils.CHARSET_ANNO),
    undefined,
    "Should not have set the charset in a private browsing window."
  );
});
