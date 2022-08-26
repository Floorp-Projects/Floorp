/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Load the shared-head file first.
/* import-globals-from ../shared-head.js */

/* exported CommonUtils, testChildAtPoint, Layout, hitTest, testOffsetAtPoint */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js",
  this
);

// Loading and common.js from accessible/tests/mochitest/ for all tests, as
// well as promisified-events.js.
loadScripts(
  { name: "common.js", dir: MOCHITESTS_DIR },
  { name: "promisified-events.js", dir: MOCHITESTS_DIR }
);

const { CommonUtils } = ChromeUtils.import(
  "chrome://mochitests/content/browser/accessible/tests/browser/Common.jsm"
);

const { Layout } = ChromeUtils.import(
  "chrome://mochitests/content/browser/accessible/tests/browser/Layout.jsm"
);

function getChildAtPoint(container, x, y, findDeepestChild) {
  try {
    return findDeepestChild
      ? container.getDeepestChildAtPoint(x, y)
      : container.getChildAtPoint(x, y);
  } catch (e) {
    // Failed to get child at point.
  }

  return null;
}

async function testChildAtPoint(dpr, x, y, container, child, grandChild) {
  const [containerX, containerY] = Layout.getBounds(container, dpr);
  x += containerX;
  y += containerY;
  await untilCacheIs(
    () => getChildAtPoint(container, x, y, false),
    child,
    `Wrong direct child accessible at the point (${x}, ${y}) of ${CommonUtils.prettyName(
      container
    )}, sought ${child ? roleToString(child.role) : "unknown"}`
  );
  await untilCacheIs(
    () => getChildAtPoint(container, x, y, true),
    grandChild,
    `Wrong deepest child accessible at the point (${x}, ${y}) of ${CommonUtils.prettyName(
      container
    )}, sought ${grandChild ? roleToString(grandChild.role) : "unknown"}`
  );
}

/**
 * Test if getChildAtPoint returns the given child and grand child accessibles
 * at coordinates of child accessible (direct and deep hit test).
 */
async function hitTest(browser, container, child, grandChild) {
  const [childX, childY] = await getContentBoundsForDOMElm(
    browser,
    getAccessibleDOMNodeID(child)
  );
  const x = childX + 1;
  const y = childY + 1;

  await untilCacheIs(
    () => getChildAtPoint(container, x, y, false),
    child,
    `Wrong direct child accessible at the point (${x}, ${y}) of ${CommonUtils.prettyName(
      container
    )}, sought ${child ? roleToString(child.role) : "unknown"}`
  );
  await untilCacheIs(
    () => getChildAtPoint(container, x, y, true),
    grandChild,
    `Wrong deepest child accessible at the point (${x}, ${y}) of ${CommonUtils.prettyName(
      container
    )}, sought ${grandChild ? roleToString(grandChild.role) : "unknown"}`
  );
}

/**
 * Test if getOffsetAtPoint returns the given text offset at given coordinates.
 */
async function testOffsetAtPoint(hyperText, x, y, coordType, expectedOffset) {
  await untilCacheIs(
    () => hyperText.getOffsetAtPoint(x, y, coordType),
    expectedOffset,
    `Wrong offset at given point (${x}, ${y}) for ${prettyName(hyperText)}`
  );
}
