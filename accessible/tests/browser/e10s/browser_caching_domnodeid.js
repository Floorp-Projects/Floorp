/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test DOM ID caching on remotes.
 */
addAccessibleTask(
  '<div id="div"></div>',
  async function(browser, accDoc) {
    const div = findAccessibleChildByID(accDoc, "div");
    ok(div, "Got accessible with 'div' ID.");

    let contentPromise = invokeContentTask(browser, [], () => {
      content.document.getElementById("div").id = "foo";
    });
    // When the cache is enabled, we don't await for content task to return
    //  because we want to exercise the untilCacheIs function and
    // demonstrate that it can await for a passing `is` test.
    if (!isCacheEnabled) {
      // However, when the cache is disabled, we must await it because there
      // will never be a cache update.
      await contentPromise;
    }

    await untilCacheIs(
      () => div.id,
      "foo",
      "ID is correct and updated in cache"
    );

    if (isCacheEnabled) {
      // Don't leave test without the content task promise resolved.
      await contentPromise;
    }
  },
  { iframe: true, remoteIframe: true }
);
