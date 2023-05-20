/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts(
  { name: "layout.js", dir: MOCHITESTS_DIR },
  { name: "role.js", dir: MOCHITESTS_DIR }
);
requestLongerTimeout(2);

const appUnitsPerDevPixel = 60;

function testCachedScrollPosition(
  acc,
  expectedX,
  expectedY,
  shouldBeEmpty = false
) {
  let cachedPosition = "";
  try {
    cachedPosition = acc.cache.getStringProperty("scroll-position");
  } catch (e) {
    info("Cache was not populated");
    // If the key doesn't exist, this frame is not scrollable.
    return shouldBeEmpty;
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
  async function (browser, docAcc) {
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
  async function (browser, docAcc) {
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
  async function (browser, docAcc) {
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

/**
 * Test position: fixed for containers that would otherwise be pruned from the
 * a11y tree.
 */
addAccessibleTask(
  `
<table id="fixed" role="presentation" style="position: fixed;">
  <tr><th>fixed</th></tr>
</table>
<div id="mutate" role="presentation">mutate</div>
<hr style="height: 200vh;">
<p>bottom</p>
  `,
  async function (browser, docAcc) {
    const fixed = findAccessibleChildByID(docAcc, "fixed");
    ok(fixed, "fixed is accessible");
    isnot(fixed.role, ROLE_TABLE, "fixed doesn't have ROLE_TABLE");
    ok(!findAccessibleChildByID(docAcc, "mutate"), "mutate inaccessible");
    info("Setting position: fixed on mutate");
    let shown = waitForEvent(EVENT_SHOW, "mutate");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("mutate").style.position = "fixed";
    });
    await shown;
    const origFixedBounds = await testBoundsWithContent(
      docAcc,
      "fixed",
      browser
    );
    const origMutateBounds = await testBoundsWithContent(
      docAcc,
      "mutate",
      browser
    );
    info("Scrolling to bottom of page");
    await invokeContentTask(browser, [], () => {
      content.window.scrollTo(0, content.document.body.scrollHeight);
    });
    await waitForContentPaint(browser);
    const newFixedBounds = await testBoundsWithContent(
      docAcc,
      "fixed",
      browser
    );
    Assert.deepEqual(
      newFixedBounds,
      origFixedBounds,
      "fixed bounds are unchanged"
    );
    const newMutateBounds = await testBoundsWithContent(
      docAcc,
      "mutate",
      browser
    );
    Assert.deepEqual(
      newMutateBounds,
      origMutateBounds,
      "mutate bounds are unchanged"
    );
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

/**
 * Test scroll offset on sticky-pos acc
 */
addAccessibleTask(
  `
  <div id="d" style="margin-top: 100px; margin-left: 75px; position:sticky; top:0px;">
    <button id="top">top</button>
  </div>
  `,
  async function (browser, docAcc) {
    const containerBounds = await testBoundsWithContent(docAcc, "d", browser);
    const e = waitForEvent(EVENT_REORDER, docAcc);
    await invokeContentTask(browser, [], () => {
      for (let i = 0; i < 1000; ++i) {
        const div = content.document.createElement("div");
        div.innerHTML = "<button>${i}</button>";
        content.document.body.append(div);
      }
    });
    await e;
    for (let id of ["d", "top"]) {
      info(`Verifying bounds for acc with ID ${id}`);
      const origBounds = await testBoundsWithContent(docAcc, id, browser);

      info("Scrolling partially");
      await invokeContentTask(browser, [], () => {
        // scroll some of the window
        content.window.scrollTo(0, 50);
      });

      await waitForContentPaint(browser);

      let newBounds = await testBoundsWithContent(docAcc, id, browser);
      is(
        origBounds[0],
        newBounds[0],
        `x coord of sticky element is unaffected by scrolling`
      );
      ok(
        origBounds[1] > newBounds[1] && newBounds[1] >= 0,
        "sticky element scrolled, but not off the page"
      );
      is(
        origBounds[2],
        newBounds[2],
        `width of sticky element is unaffected by scrolling`
      );
      is(
        origBounds[3],
        newBounds[3],
        `height of sticky element is unaffected by scrolling`
      );

      info("Scrolling to bottom");
      await invokeContentTask(browser, [], () => {
        // scroll to the bottom of the page
        content.window.scrollTo(0, content.document.body.scrollHeight);
      });

      await waitForContentPaint(browser);

      newBounds = await testBoundsWithContent(docAcc, id, browser);
      is(
        origBounds[0],
        newBounds[0],
        `x coord of sticky element is unaffected by scrolling`
      );
      // Subtract margin from container screen coords to get chrome height
      // which is where our y pos should be
      is(
        newBounds[1],
        containerBounds[1] - 100,
        "Sticky element is top of screen"
      );
      is(
        origBounds[2],
        newBounds[2],
        `width of sticky element is unaffected by scrolling`
      );
      is(
        origBounds[3],
        newBounds[3],
        `height of sticky element is unaffected by scrolling`
      );

      info("Removing position style on container");
      await invokeContentTask(browser, [], () => {
        // remove position styling
        content.document.getElementById("d").style =
          "margin-top: 100px; margin-left: 75px;";
      });

      await waitForContentPaint(browser);

      newBounds = await testBoundsWithContent(docAcc, id, browser);

      is(
        origBounds[0],
        newBounds[0],
        `x coord of non-sticky element remains accurate.`
      );
      ok(newBounds[1] < 0, "y coordinate shows item scrolled off page");

      // Removing the position styling on this acc causes it to be bound by
      // its parent's bounding box, which alters its width as a block element.
      // We don't particularly care about width in this test, so skip it.
      is(
        origBounds[3],
        newBounds[3],
        `height of non-sticky element remains accurate.`
      );

      info("Adding position style on container");
      await invokeContentTask(browser, [], () => {
        // re-add position styling
        content.document.getElementById("d").style =
          "margin-top: 100px; margin-left: 75px; position:sticky; top:0px;";
      });

      await waitForContentPaint(browser);

      newBounds = await testBoundsWithContent(docAcc, id, browser);
      is(
        origBounds[0],
        newBounds[0],
        `x coord of sticky element is unaffected by scrolling`
      );
      is(
        newBounds[1],
        containerBounds[1] - 100,
        "Sticky element is top of screen"
      );
      is(
        origBounds[2],
        newBounds[2],
        `width of sticky element is unaffected by scrolling`
      );
      is(
        origBounds[3],
        newBounds[3],
        `height of sticky element is unaffected by scrolling`
      );

      info("Scrolling back up to test next ID");
      await invokeContentTask(browser, [], () => {
        // scroll some of the window
        content.window.scrollTo(0, 0);
      });
    }
  },
  { chrome: false, iframe: false, remoteIframe: false }
);

/**
 * Test position: sticky for containers that would otherwise be pruned from the
 * a11y tree.
 */
addAccessibleTask(
  `
<hr style="height: 100vh;">
<div id="stickyContainer">
  <div id="sticky" role="presentation" style="position: sticky; top: 0px;">sticky</div>
  <hr style="height: 100vh;">
  <p id="stickyEnd">stickyEnd</p>
</div>
<div id="mutateContainer">
  <div id="mutate" role="presentation" style="top: 0px;">mutate</div>
  <hr style="height: 100vh;">
  <p id="mutateEnd">mutateEnd</p>
</div>
  `,
  async function (browser, docAcc) {
    ok(findAccessibleChildByID(docAcc, "sticky"), "sticky is accessible");
    info("Scrolling to sticky");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("sticky").scrollIntoView();
    });
    await waitForContentPaint(browser);
    const origStickyBounds = await testBoundsWithContent(
      docAcc,
      "sticky",
      browser
    );
    info("Scrolling to stickyEnd");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("stickyEnd").scrollIntoView();
    });
    await waitForContentPaint(browser);
    const newStickyBounds = await testBoundsWithContent(
      docAcc,
      "sticky",
      browser
    );
    Assert.deepEqual(
      newStickyBounds,
      origStickyBounds,
      "sticky bounds are unchanged"
    );

    ok(!findAccessibleChildByID(docAcc, "mutate"), "mutate inaccessible");
    info("Setting position: sticky on mutate");
    let shown = waitForEvent(EVENT_SHOW, "mutate");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("mutate").style.position = "sticky";
    });
    await shown;
    info("Scrolling to mutate");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("mutate").scrollIntoView();
    });
    await waitForContentPaint(browser);
    const origMutateBounds = await testBoundsWithContent(
      docAcc,
      "mutate",
      browser
    );
    info("Scrolling to mutateEnd");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("mutateEnd").scrollIntoView();
    });
    await waitForContentPaint(browser);
    const newMutateBounds = await testBoundsWithContent(
      docAcc,
      "mutate",
      browser
    );
    assertBoundsFuzzyEqual(newMutateBounds, origMutateBounds);
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

/**
 * Test scroll offset on non-scrollable accs
 */
addAccessibleTask(
  `
  <div id='square' style='height:100px; width: 100px; background:green;'>hello world
  </div>
  `,
  async function (browser, docAcc) {
    const square = findAccessibleChildByID(docAcc, "square");
    await untilCacheOk(
      () => testCachedScrollPosition(square, 0, 0, true),
      "Square is not scrollable."
    );

    info("Adding more text content to square");
    await invokeContentTask(browser, [], () => {
      const s = content.document.getElementById("square");
      s.textContent =
        "hello world I am some text and I should overflow this container because I am very long";
      s.offsetTop; // Flush layout.
    });

    await waitForContentPaint(browser);

    await untilCacheOk(
      () => testCachedScrollPosition(square, 0, 0, true),
      "Square is not scrollable (still has overflow:visible)."
    );

    info("Adding overflow:auto; styling");
    await invokeContentTask(browser, [], () => {
      const s = content.document.getElementById("square");
      s.setAttribute(
        "style",
        "overflow:auto; height:100px; width: 100px; background:green;"
      );
      s.offsetTop; // Flush layout.
    });

    await waitForContentPaint(browser);

    await untilCacheOk(
      () => testCachedScrollPosition(square, 0, 0),
      "Square is scrollable."
    );
  },
  { iframe: true, remoteIframe: true }
);
