/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that getBoxQuadsFromWindowOrigin works across process
// boundaries.
//
// The test forces a fission window, because there is some
// imprecision in the APZ transforms for non-fission windows,
// and the getBoxQuadsFromWindowOrigin is designed specifically
// to be used by a fission browser.
//
// This test embeds a number of documents within iframes,
// with a variety of borders and padding. Each iframe hosts
// a document in different domain than its container.
//
// The innermost documents have a 50px x 50px div with a
// 50px margin. So relative to its own iframe, the offset
// for the div should be 50, 50.
//
// Here's the embedding diagram:
// +-- A -----------------------------------------------------+
// |                                                          |
// | +- div -----+                                            |
// | | 100 x 100 |                                            |
// | +-----------+                                            |
// |                                                          |
// | +- div 20px margin ------------------------------------+ |
// | |                                                      | |
// | | +- B: iframe 10px border -+ +- D: iframe 40px pad -+ | |
// | | |     250 x 250           | |     250 x 250        | | |
// | | |     50px margin         | |     50px margin      | | |
// | | |                         | |                      | | |
// | | | +- C: iframe ---+       | | +- E: iframe ---+    | | |
// | | | |     150 x 150 |       | | |     150 x 150 |    | | |
// | | | +---------------+       | | +---------------+    | | |
// | | +-------------------------+ +----------------------+ | |
// | +------------------------------------------------------+ |
// +----------------------------------------------------------+
//
// The following checks are made:
// C-div relative to A should have offset 130, 230.
// E-div relative to A should have offset 430, 260.
//
// This tests the most likely cases for the code that handles
// relativeToTopLevelBrowsingContextId. It does not check these
// cases:
// 1) A css-transform'ed iframe.
// 2) An abspos iframe.
// 3) An iframe embedded in an SVG.

"use strict";
/* import-globals-from ../../../../gfx/layers/apz/test/mochitest/apz_test_utils.js */

const TEST_URI = TEST_URI_ROOT + "doc_layoutHelpers_getBoxQuads2-a.html";

add_task(async function() {
  info("Opening a fission window.");
  const fissionWin = await BrowserTestUtils.openNewBrowserWindow({
    remote: true,
    fission: true,
  });

  loadHelperScript(
    "../../../../gfx/layers/apz/test/mochitest/apz_test_utils.js"
  );
  loadHelperScript("../../../../../tests/SimpleTest/paint_listener.js");

  const tab = await BrowserTestUtils.openNewForegroundTab(
    fissionWin.gBrowser,
    TEST_URI
  );

  info("Running tests");

  ok(waitUntilApzStable, "waitUntilApzStable is defined.");
  await waitUntilApzStable();

  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    const win = content.window;
    const doc = content.document;
    const refNode = doc.documentElement;
    const iframeB = doc.getElementById("b");
    const iframeD = doc.getElementById("d");

    // Get the offset of the reference node to the window origin. We'll use
    // this offset later to adjust the quads we get from the iframes.
    const refQuad = refNode.getBoxQuadsFromWindowOrigin()[0];
    const offsetX = refQuad.p1.x;
    const offsetY = refQuad.p1.y;
    info(`Reference node is offset (${offsetX}, ${offsetY}) from window.`);

    function postAndReceiveMessage(iframe) {
      return new Promise(resolve => {
        const onmessage = event => {
          if (event.data.quad) {
            win.removeEventListener("message", onmessage);
            resolve(event.data.quad);
          }
        };
        win.addEventListener("message", onmessage, { capture: false });
        iframe.contentWindow.postMessage({ callGetBoxQuads: true }, "*");
      });
    }

    // Bug 1624659: Our checks are not always precise, though for these test
    // cases they should align precisely to css pixels. For now we use an
    // epsilon and a locally-defined isfuzzy to compensate. We can't use
    // SimpleTest.isfuzzy, because it's not bridged to the ContentTask.
    // If that is ever bridged, we can remove the isfuzzy definition here and
    // everything should "just work".
    function isfuzzy(actual, expected, epsilon, msg) {
      if (actual >= expected - epsilon && actual <= expected + epsilon) {
        ok(true, msg);
      } else {
        // This will trigger the usual failure message for is.
        is(actual, expected, msg);
      }
    }

    const ADDITIVE_EPSILON = 1;
    const checksToMake = [
      {
        msg: "C-div",
        iframe: iframeB,
        left: 130,
        top: 230,
        right: 180,
        bottom: 280,
      },
      {
        msg: "E-div",
        iframe: iframeD,
        left: 430,
        top: 260,
        right: 480,
        bottom: 310,
      },
    ];

    for (const { msg, iframe, left, top, right, bottom } of checksToMake) {
      info("Checking " + msg + ".");
      const quad = await postAndReceiveMessage(iframe);
      const bounds = quad.getBounds();
      info(
        `Quad bounds is (${bounds.left}, ${bounds.top}) to (${bounds.right}, ${bounds.bottom}).`
      );
      isfuzzy(
        bounds.left - offsetX,
        left,
        ADDITIVE_EPSILON,
        msg + " quad left position is as expected."
      );
      isfuzzy(
        bounds.top - offsetY,
        top,
        ADDITIVE_EPSILON,
        msg + " quad top position is as expected."
      );
      isfuzzy(
        bounds.right - offsetX,
        right,
        ADDITIVE_EPSILON,
        msg + " quad right position is as expected."
      );
      isfuzzy(
        bounds.bottom - offsetY,
        bottom,
        ADDITIVE_EPSILON,
        msg + " quad bottom position is as expected."
      );
    }
  });

  fissionWin.gBrowser.removeCurrentTab();

  await BrowserTestUtils.closeWindow(fissionWin);

  // Clean up the properties added to window by paint_listener.js.
  delete window.waitForAllPaintsFlushed;
  delete window.waitForAllPaints;
  delete window.promiseAllPaintsDone;
});
