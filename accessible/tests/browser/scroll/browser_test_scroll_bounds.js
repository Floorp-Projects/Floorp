/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/layout.js */
loadScripts({ name: "layout.js", dir: MOCHITESTS_DIR });
requestLongerTimeout(2);

const appUnitsPerDevPixel = 60;

function testCachedScrollPosition(acc, expectedX, expectedY) {
  let cachedPosition = "";
  try {
    cachedPosition = acc.cache.getStringProperty("scroll-position");
  } catch (e) {
    // If the key doesn't exist, this means 0, 0.
    cachedPosition = "0, 0";
  }

  // The value we retrieve from the cache is in app units, but the values
  // passed in are in pixels. Since the retrieved value is a string,
  // and harder to modify, adjust our expected x and y values to match its units.
  return (
    cachedPosition ==
    `${expectedX * appUnitsPerDevPixel}, ${expectedY * appUnitsPerDevPixel}`
  );
}

function getCachedBounds(acc) {
  let cachedBounds = "";
  try {
    cachedBounds = acc.cache.getStringProperty("relative-bounds");
  } catch (e) {
    ok(false, "Unable to fetch cached bounds from cache!");
  }
  return cachedBounds;
}

/**
 * Test bounds of accessibles after scrolling
 */
addAccessibleTask(
  `
  <div id='square' style='height:100px; width:100px; background:green; margin-top:3000px; margin-bottom:4000px;'>
  </div>

  <div id='rect' style='height:40px; width:200px; background:blue; margin-bottom:3400px'>
  </div>
  `,
  async function(browser, docAcc) {
    ok(docAcc, "iframe document acc is present");
    await testBoundsWithContent(docAcc, "square", browser);
    await testBoundsWithContent(docAcc, "rect", browser);

    await invokeContentTask(browser, [], () => {
      content.document.getElementById("square").scrollIntoView();
    });

    await waitForContentPaint(browser);

    await testBoundsWithContent(docAcc, "square", browser);
    await testBoundsWithContent(docAcc, "rect", browser);

    // Scroll rect into view, but also make it reflow so we can be sure the
    // bounds are correct for reflowed frames.
    await invokeContentTask(browser, [], () => {
      const rect = content.document.getElementById("rect");
      rect.scrollIntoView();
      rect.style.width = "300px";
      rect.offsetTop; // Flush layout.
      rect.style.width = "200px";
      rect.offsetTop; // Flush layout.
    });

    await waitForContentPaint(browser);
    await testBoundsWithContent(docAcc, "square", browser);
    await testBoundsWithContent(docAcc, "rect", browser);
  },
  { iframe: true, remoteIframe: true, chrome: true }
);

/**
 * Test scroll offset on cached accessibles
 */
addAccessibleTask(
  `
  <div id='square' style='height:100px; width:100px; background:green; margin-top:3000px; margin-bottom:4000px;'>
  </div>

  <div id='rect' style='height:40px; width:200px; background:blue; margin-bottom:3400px'>
  </div>
  `,
  async function(browser, docAcc) {
    // We can only access the `cache` attribute of an accessible when
    // the cache is enabled and we're in a remote browser. Verify
    // both these conditions hold, and return early if they don't.
    if (!isCacheEnabled || !browser.isRemoteBrowser) {
      return;
    }

    ok(docAcc, "iframe document acc is present");
    await untilCacheOk(
      () => testCachedScrollPosition(docAcc, 0, 0),
      "Correct initial scroll position."
    );
    const rectAcc = findAccessibleChildByID(docAcc, "rect");
    const rectInitialBounds = getCachedBounds(rectAcc);

    await invokeContentTask(browser, [], () => {
      content.document.getElementById("square").scrollIntoView();
    });

    await waitForContentPaint(browser);

    // The only content to scroll over is `square`'s top margin
    // so our scroll offset here should be 3000px
    await untilCacheOk(
      () => testCachedScrollPosition(docAcc, 0, 3000),
      "Correct scroll position after first scroll."
    );

    // Scroll rect into view, but also make it reflow so we can be sure the
    // bounds are correct for reflowed frames.
    await invokeContentTask(browser, [], () => {
      const rect = content.document.getElementById("rect");
      rect.scrollIntoView();
      rect.style.width = "300px";
      rect.offsetTop;
      rect.style.width = "200px";
    });

    await waitForContentPaint(browser);
    // We have to scroll over `square`'s top margin (3000px),
    // `square` itself (100px), and `square`'s bottom margin (4000px).
    // This should give us a 7100px offset.
    await untilCacheOk(
      () => testCachedScrollPosition(docAcc, 0, 7100),
      "Correct final scroll position."
    );
    await untilCacheIs(
      () => getCachedBounds(rectAcc),
      rectInitialBounds,
      "Cached relative bounds don't change when scrolling"
    );
  },
  { iframe: true, remoteIframe: true }
);

/**
 * Test scroll offset fixed-pos acc accs
 */
addAccessibleTask(
  `
  <div style="margin-top: 100px; margin-left: 75px; border: 1px solid;">
    <div id="d" style="position:fixed;">
      <button id="top">top</button>
    </div>
  </div>
  `,
  async function(browser, docAcc) {
    const origTopBounds = await testBoundsWithContent(docAcc, "top", browser);
    const origDBounds = await testBoundsWithContent(docAcc, "d", browser);
    const e = waitForEvent(EVENT_REORDER, docAcc);
    await invokeContentTask(browser, [], () => {
      for (let i = 0; i < 1000; ++i) {
        const div = content.document.createElement("div");
        div.innerHTML = "<button>${i}</button>";
        content.document.body.append(div);
      }
    });
    await e;

    await invokeContentTask(browser, [], () => {
      // scroll to the bottom of the page
      content.window.scrollTo(0, content.document.body.scrollHeight);
    });

    await waitForContentPaint(browser);

    let newTopBounds = await testBoundsWithContent(docAcc, "top", browser);
    let newDBounds = await testBoundsWithContent(docAcc, "d", browser);
    is(
      origTopBounds[0],
      newTopBounds[0],
      "x of fixed elem is unaffected by scrolling"
    );
    is(
      origTopBounds[1],
      newTopBounds[1],
      "y of fixed elem is unaffected by scrolling"
    );
    is(
      origTopBounds[2],
      newTopBounds[2],
      "width of fixed elem is unaffected by scrolling"
    );
    is(
      origTopBounds[3],
      newTopBounds[3],
      "height of fixed elem is unaffected by scrolling"
    );
    is(
      origDBounds[0],
      newTopBounds[0],
      "x of fixed elem container is unaffected by scrolling"
    );
    is(
      origDBounds[1],
      newDBounds[1],
      "y of fixed elem container is unaffected by scrolling"
    );
    is(
      origDBounds[2],
      newDBounds[2],
      "width of fixed container elem is unaffected by scrolling"
    );
    is(
      origDBounds[3],
      newDBounds[3],
      "height of fixed container elem is unaffected by scrolling"
    );

    await invokeContentTask(browser, [], () => {
      // remove position styling
      content.document.getElementById("d").style = "";
    });

    await waitForContentPaint(browser);

    newTopBounds = await testBoundsWithContent(docAcc, "top", browser);
    newDBounds = await testBoundsWithContent(docAcc, "d", browser);
    is(
      origTopBounds[0],
      newTopBounds[0],
      "x of non-fixed element remains accurate."
    );
    ok(newTopBounds[1] < 0, "y coordinate shows item scrolled off page");
    is(
      origTopBounds[2],
      newTopBounds[2],
      "width of non-fixed element remains accurate."
    );
    is(
      origTopBounds[3],
      newTopBounds[3],
      "height of non-fixed element remains accurate."
    );
    is(
      origDBounds[0],
      newDBounds[0],
      "x of non-fixed container element remains accurate."
    );
    ok(newDBounds[1] < 0, "y coordinate shows container scrolled off page");
    // Removing the position styling on this acc causes it to be bound by
    // its parent's bounding box, which alters its width as a block element.
    // We don't particularly care about width in this test, so skip it.
    is(
      origDBounds[3],
      newDBounds[3],
      "height of non-fixed container element remains accurate."
    );

    await invokeContentTask(browser, [], () => {
      // re-add position styling
      content.document.getElementById("d").style = "position:fixed;";
    });

    await waitForContentPaint(browser);

    newTopBounds = await testBoundsWithContent(docAcc, "top", browser);
    newDBounds = await testBoundsWithContent(docAcc, "d", browser);
    is(
      origTopBounds[0],
      newTopBounds[0],
      "x correct when position:fixed is added."
    );
    is(
      origTopBounds[1],
      newTopBounds[1],
      "y correct when position:fixed is added."
    );
    is(
      origTopBounds[2],
      newTopBounds[2],
      "width correct when position:fixed is added."
    );
    is(
      origTopBounds[3],
      newTopBounds[3],
      "height correct when position:fixed is added."
    );
    is(
      origDBounds[0],
      newDBounds[0],
      "x of container correct when position:fixed is added."
    );
    is(
      origDBounds[1],
      newDBounds[1],
      "y of container correct when position:fixed is added."
    );
    is(
      origDBounds[2],
      newDBounds[2],
      "width of container correct when position:fixed is added."
    );
    is(
      origDBounds[3],
      newDBounds[3],
      "height of container correct when position:fixed is added."
    );
  },
  { chrome: true, iframe: true, remoteIframe: true }
);
