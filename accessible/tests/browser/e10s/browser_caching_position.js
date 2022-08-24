/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/layout.js */
loadScripts({ name: "layout.js", dir: MOCHITESTS_DIR });

function getCachedBounds(acc) {
  let cachedBounds = "";
  try {
    cachedBounds = acc.cache.getStringProperty("relative-bounds");
  } catch (e) {
    ok(false, "Unable to fetch cached bounds from cache!");
  }
  return cachedBounds;
}

async function testCoordinates(accDoc, id, expectedWidthPx, expectedHeightPx) {
  let acc = findAccessibleChildByID(accDoc, id, [Ci.nsIAccessibleImage]);
  if (!acc) {
    return;
  }

  let screenX = {};
  let screenY = {};
  let windowX = {};
  let windowY = {};
  let parentX = {};
  let parentY = {};

  // get screen coordinates.
  acc.getImagePosition(
    nsIAccessibleCoordinateType.COORDTYPE_SCREEN_RELATIVE,
    screenX,
    screenY
  );
  // get window coordinates.
  acc.getImagePosition(
    nsIAccessibleCoordinateType.COORDTYPE_WINDOW_RELATIVE,
    windowX,
    windowY
  );
  // get parent related coordinates.
  acc.getImagePosition(
    nsIAccessibleCoordinateType.COORDTYPE_PARENT_RELATIVE,
    parentX,
    parentY
  );
  // XXX For linked images, a negative parentY value is returned, and the
  // screenY coordinate is the link's screenY coordinate minus 1.
  // Until this is fixed, set parentY to -1 if it's negative.
  if (parentY.value < 0) {
    parentY.value = -1;
  }

  // See if asking image for child at image's screen coordinates gives
  // correct accessible. getChildAtPoint operates on screen coordinates.
  let tempAcc = null;
  try {
    tempAcc = acc.getChildAtPoint(screenX.value, screenY.value);
  } catch (e) {}
  is(tempAcc, acc, "Wrong accessible returned for position of " + id + "!");

  // get image's parent.
  let imageParentAcc = null;
  try {
    imageParentAcc = acc.parent;
  } catch (e) {}
  ok(imageParentAcc, "no parent accessible for " + id + "!");

  if (imageParentAcc) {
    // See if parent's screen coordinates plus image's parent relative
    // coordinates equal to image's screen coordinates.
    let parentAccX = {};
    let parentAccY = {};
    let parentAccWidth = {};
    let parentAccHeight = {};
    imageParentAcc.getBounds(
      parentAccX,
      parentAccY,
      parentAccWidth,
      parentAccHeight
    );
    is(
      parentAccX.value + parentX.value,
      screenX.value,
      "Wrong screen x coordinate for " + id + "!"
    );
    // XXX see bug 456344
    // is(
    //   parentAccY.value + parentY.value,
    //   screenY.value,
    //   "Wrong screen y coordinate for " + id + "!"
    // );
  }

  let [expectedW, expectedH] = CSSToDevicePixels(
    window,
    expectedWidthPx,
    expectedHeightPx
  );
  let width = {};
  let height = {};
  acc.getImageSize(width, height);
  is(width.value, expectedW, "Wrong width for " + id + "!");
  is(height.value, expectedH, "wrong height for " + id + "!");
}

addAccessibleTask(
  `
  <br>Simple image:<br>
  <img id="nonLinkedImage" src="http://example.com/a11y/accessible/tests/mochitest/moz.png"/>
  <br>Linked image:<br>
  <a href="http://www.mozilla.org"><img id="linkedImage" src="http://example.com/a11y/accessible/tests/mochitest/moz.png"></a>
  <br>Image with longdesc:<br>
  <img id="longdesc" src="http://example.com/a11y/accessible/tests/mochitest/moz.png" longdesc="longdesc_src.html"
       alt="Image of Mozilla logo"/>
  <br>Image with invalid url in longdesc:<br>
  <img id="invalidLongdesc" src="http://example.com/a11y/accessible/tests/mochitest/moz.png" longdesc="longdesc src.html"
       alt="Image of Mozilla logo"/>
  <br>Image with click and longdesc:<br>
  <img id="clickAndLongdesc" src="http://example.com/a11y/accessible/tests/mochitest/moz.png" longdesc="longdesc_src.html"
       alt="Another image of Mozilla logo" onclick="alert('Clicked!');"/>

  <br>image described by a link to be treated as longdesc<br>
  <img id="longdesc2" src="http://example.com/a11y/accessible/tests/mochitest/moz.png" aria-describedby="describing_link"
       alt="Second Image of Mozilla logo"/>
  <a id="describing_link" href="longdesc_src.html">link to description of image</a>

  <br>Image described by a link to be treated as longdesc with whitespaces<br>
  <img id="longdesc3" src="http://example.com/a11y/accessible/tests/mochitest/moz.png" aria-describedby="describing_link2"
       alt="Second Image of Mozilla logo"/>
  <a id="describing_link2" href="longdesc src.html">link to description of image</a>

  <br>Image with click:<br>
  <img id="click" src="http://example.com/a11y/accessible/tests/mochitest/moz.png"
       alt="A third image of Mozilla logo" onclick="alert('Clicked, too!');"/>
  `,
  async function(browser, docAcc) {
    // Test non-linked image
    await testCoordinates(docAcc, "nonLinkedImage", 89, 38);

    // Test linked image
    await testCoordinates(docAcc, "linkedImage", 89, 38);

    // Image with long desc
    await testCoordinates(docAcc, "longdesc", 89, 38);

    // Image with invalid url in long desc
    await testCoordinates(docAcc, "invalidLongdesc", 89, 38);

    // Image with click and long desc
    await testCoordinates(docAcc, "clickAndLongdesc", 89, 38);

    // Image with click
    await testCoordinates(docAcc, "click", 89, 38);

    // Image with long desc
    await testCoordinates(docAcc, "longdesc2", 89, 38);

    // Image described by HTML:a@href with whitespaces
    await testCoordinates(docAcc, "longdesc3", 89, 38);
  }
);

addAccessibleTask(
  `
  <br>Linked image:<br>
  <a href="http://www.mozilla.org"><img id="linkedImage" src="http://example.com/a11y/accessible/tests/mochitest/moz.png"></a>
  `,
  async function(browser, docAcc) {
    const imgAcc = findAccessibleChildByID(docAcc, "linkedImage", [
      Ci.nsIAccessibleImage,
    ]);
    const origCachedBounds = getCachedBounds(imgAcc);

    await invokeContentTask(browser, [], () => {
      const imgNode = content.document.getElementById("linkedImage");
      imgNode.style = "margin-left: 1000px; margin-top: 500px;";
    });

    await untilCacheOk(() => {
      return origCachedBounds != getCachedBounds(imgAcc);
    }, "Cached bounds update after mutation");
  },
  {
    // We can only access the `cache` attribute of an accessible when
    // the cache is enabled and we're in a remote browser.
    topLevel: isCacheEnabled,
    iframe: isCacheEnabled,
  }
);
