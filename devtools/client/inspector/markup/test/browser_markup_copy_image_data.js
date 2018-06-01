/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that image nodes have the "copy data-uri" contextual menu item enabled
// and that clicking it puts the image data into the clipboard

add_task(async function() {
  await addTab(URL_ROOT + "doc_markup_image_and_canvas.html");
  const {inspector, testActor} = await openInspector();

  await selectNode("div", inspector);
  await assertCopyImageDataNotAvailable(inspector);

  await selectNode("img", inspector);
  await assertCopyImageDataAvailable(inspector);
  const expectedSrc = await testActor.getAttribute("img", "src");
  await triggerCopyImageUrlAndWaitForClipboard(expectedSrc, inspector);

  await selectNode("canvas", inspector);
  await assertCopyImageDataAvailable(inspector);
  const expectedURL = await testActor.eval(`
    document.querySelector(".canvas").toDataURL();`);
  await triggerCopyImageUrlAndWaitForClipboard(expectedURL, inspector);

  // Check again that the menu isn't available on the DIV (to make sure our
  // menu updating mechanism works)
  await selectNode("div", inspector);
  await assertCopyImageDataNotAvailable(inspector);
});

function assertCopyImageDataNotAvailable(inspector) {
  const allMenuItems = openContextMenuAndGetAllItems(inspector);
  const item = allMenuItems.find(i => i.id === "node-menu-copyimagedatauri");

  ok(item, "The menu item was found in the contextual menu");
  ok(item.disabled, "The menu item is disabled");
}

function assertCopyImageDataAvailable(inspector) {
  const allMenuItems = openContextMenuAndGetAllItems(inspector);
  const item = allMenuItems.find(i => i.id === "node-menu-copyimagedatauri");

  ok(item, "The menu item was found in the contextual menu");
  ok(!item.disabled, "The menu item is enabled");
}

function triggerCopyImageUrlAndWaitForClipboard(expected, inspector) {
  return new Promise(resolve => {
    SimpleTest.waitForClipboard(expected, () => {
      inspector.markup.getContainer(inspector.selection.nodeFront)
                      .copyImageDataUri();
    }, () => {
      ok(true, "The clipboard contains the expected value " +
               expected.substring(0, 50) + "...");
      resolve();
    }, () => {
      ok(false, "The clipboard doesn't contain the expected value " +
                expected.substring(0, 50) + "...");
      resolve();
    });
  });
}
