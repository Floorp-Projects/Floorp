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
  let menu = yield openNodeMenu(inspector);

  let item = menu.getElementsByAttribute("id", "node-menu-copyimagedatauri")[0];
  ok(item, "The menu item was found in the contextual menu");
  is(item.getAttribute("disabled"), "true", "The menu item is disabled");

  yield closeNodeMenu(inspector);
}

function* assertCopyImageDataAvailable(inspector) {
  let menu = yield openNodeMenu(inspector);

  let item = menu.getElementsByAttribute("id", "node-menu-copyimagedatauri")[0];
  ok(item, "The menu item was found in the contextual menu");
  is(item.getAttribute("disabled"), "", "The menu item is enabled");

  yield closeNodeMenu(inspector);
}

function* openNodeMenu(inspector) {
  let onShown = once(inspector.nodemenu, "popupshown", false);
  inspector.nodemenu.hidden = false;
  inspector.nodemenu.openPopup();
  yield onShown;
  return inspector.nodemenu;
}

function* closeNodeMenu(inspector) {
  let onHidden = once(inspector.nodemenu, "popuphidden", false);
  inspector.nodemenu.hidden = true;
  inspector.nodemenu.hidePopup();
  yield onHidden;
  return inspector.nodemenu;
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
