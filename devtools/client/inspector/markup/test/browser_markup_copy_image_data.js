/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that image nodes have the "copy data-uri" contextual menu item enabled
// and that clicking it puts the image data into the clipboard

add_task(function* () {
  yield addTab(URL_ROOT + "doc_markup_image_and_canvas.html");
  let {inspector, testActor} = yield openInspector();

  yield selectNode("div", inspector);
  yield assertCopyImageDataNotAvailable(inspector);

  yield selectNode("img", inspector);
  yield assertCopyImageDataAvailable(inspector);
  let expectedSrc = yield testActor.getAttribute("img", "src");
  yield triggerCopyImageUrlAndWaitForClipboard(expectedSrc, inspector);

  yield selectNode("canvas", inspector);
  yield assertCopyImageDataAvailable(inspector);
  let expectedURL = yield testActor.eval(`
    content.document.querySelector(".canvas").toDataURL();`);
  yield triggerCopyImageUrlAndWaitForClipboard(expectedURL, inspector);

  // Check again that the menu isn't available on the DIV (to make sure our
  // menu updating mechanism works)
  yield selectNode("div", inspector);
  yield assertCopyImageDataNotAvailable(inspector);
});

function* assertCopyImageDataNotAvailable(inspector) {
  let allMenuItems = openContextMenuAndGetAllItems(inspector);
  let item = allMenuItems.find(i => i.id === "node-menu-copyimagedatauri");

  ok(item, "The menu item was found in the contextual menu");
  ok(item.disabled, "The menu item is disabled");
}

function* assertCopyImageDataAvailable(inspector) {
  let allMenuItems = openContextMenuAndGetAllItems(inspector);
  let item = allMenuItems.find(i => i.id === "node-menu-copyimagedatauri");

  ok(item, "The menu item was found in the contextual menu");
  ok(!item.disabled, "The menu item is enabled");
}

function triggerCopyImageUrlAndWaitForClipboard(expected, inspector) {
  let def = promise.defer();

  SimpleTest.waitForClipboard(expected, () => {
    inspector.markup.getContainer(inspector.selection.nodeFront)
                    .copyImageDataUri();
  }, () => {
    ok(true, "The clipboard contains the expected value " +
             expected.substring(0, 50) + "...");
    def.resolve();
  }, () => {
    ok(false, "The clipboard doesn't contain the expected value " +
              expected.substring(0, 50) + "...");
    def.resolve();
  });

  return def.promise;
}
