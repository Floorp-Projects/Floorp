/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that getBoxQuadsFromWindowOrigin works across process
// boundaries.
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

const TEST_URI = TEST_URI_ROOT + "doc_layoutHelpers_getBoxQuads2-a.html";

add_task(async function() {
  const tab = await addTab(TEST_URI);

  info("Running tests");

  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    const win = content.window;
    const doc = content.document;
    const docNode = doc.documentElement;
    const iframeB = doc.getElementById("b");
    const iframeD = doc.getElementById("d");

    // Get the offset of the root document to the window origin. We'll
    // use this later to adjust the quads we get from the iframes.
    const docQuad = docNode.getBoxQuadsFromWindowOrigin()[0];
    const offsetX = docQuad.p1.x;
    const offsetY = docQuad.p1.y;
    info(`Document is offset ${offsetX}, ${offsetY} from window.`);

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

    const checksToMake = [
      { msg: "C-div", iframe: iframeB, x: 130, y: 230 },
      { msg: "E-div", iframe: iframeD, x: 430, y: 260 },
    ];

    for (const { msg, iframe, x, y } of checksToMake) {
      info("Checking " + msg + ".");
      const quad = await postAndReceiveMessage(iframe);
      is(
        Math.round(quad.p1.x - offsetX),
        x,
        msg + " quad x position is as expected."
      );
      is(
        Math.round(quad.p1.y - offsetY),
        y,
        msg + " quad y position is as expected."
      );
    }
  });

  gBrowser.removeCurrentTab();
});
