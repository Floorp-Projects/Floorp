/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* Tests both Copy URL and Copy Data URL context menu items */

const TEST_DATA_URI = "data:image/gif;base64,R0lGODlhAQABAIAAAP///wAAACwAAAAAAQABAAACAkQBADs=";

// Invalid URL still needs to be reachable otherwise getImageDataUrl will
// timeout.  DevTools chrome:// URLs aren't content accessible, so use some
// random resource:// URL here.
const INVALID_IMAGE_URI = "resource://devtools/client/definitions.js";
const ERROR_MESSAGE = STYLE_INSPECTOR_L10N.getStr("styleinspector.copyImageDataUrlError");

add_task(async function() {
  const TEST_URI = `<style type="text/css">
      .valid-background {
        background-image: url(${TEST_DATA_URI});
      }
      .invalid-background {
        background-image: url(${INVALID_IMAGE_URI});
      }
    </style>
    <div class="valid-background">Valid background image</div>
    <div class="invalid-background">Invalid background image</div>`;

  await addTab("data:text/html;charset=utf8," + encodeURIComponent(TEST_URI));

  await startTest();
});

async function startTest() {
  info("Opening rule view");
  let {inspector, view} = await openRuleView();

  info("Test valid background image URL in rule view");
  await testCopyUrlToClipboard({view, inspector}, "data-uri",
    ".valid-background", TEST_DATA_URI);
  await testCopyUrlToClipboard({view, inspector}, "url",
    ".valid-background", TEST_DATA_URI);

  info("Test invalid background image URL in rue view");
  await testCopyUrlToClipboard({view, inspector}, "data-uri",
    ".invalid-background", ERROR_MESSAGE);
  await testCopyUrlToClipboard({view, inspector}, "url",
    ".invalid-background", INVALID_IMAGE_URI);

  info("Opening computed view");
  view = selectComputedView(inspector);

  info("Test valid background image URL in computed view");
  await testCopyUrlToClipboard({view, inspector}, "data-uri",
    ".valid-background", TEST_DATA_URI);
  await testCopyUrlToClipboard({view, inspector}, "url",
    ".valid-background", TEST_DATA_URI);

  info("Test invalid background image URL in computed view");
  await testCopyUrlToClipboard({view, inspector}, "data-uri",
    ".invalid-background", ERROR_MESSAGE);
  await testCopyUrlToClipboard({view, inspector}, "url",
    ".invalid-background", INVALID_IMAGE_URI);
}

async function testCopyUrlToClipboard({view, inspector}, type, selector, expected) {
  info("Select node in inspector panel");
  await selectNode(selector, inspector);

  info("Retrieve background-image link for selected node in current " +
       "styleinspector view");
  const property = getBackgroundImageProperty(view, selector);
  const imageLink = property.valueSpan.querySelector(".theme-link");
  ok(imageLink, "Background-image link element found");

  info("Simulate right click on the background-image URL");
  const allMenuItems = openStyleContextMenuAndGetAllItems(view, imageLink);
  const menuitemCopyUrl = allMenuItems.find(item => item.label ===
    STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copyUrl"));
  const menuitemCopyImageDataUrl = allMenuItems.find(item => item.label ===
    STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copyImageDataUrl"));

  info("Context menu is displayed");
  ok(menuitemCopyUrl.visible,
     "\"Copy URL\" menu entry is displayed");
  ok(menuitemCopyImageDataUrl.visible,
     "\"Copy Image Data-URL\" menu entry is displayed");

  if (type == "data-uri") {
    info("Click Copy Data URI and wait for clipboard");
    await waitForClipboardPromise(() => {
      return menuitemCopyImageDataUrl.click();
    }, expected);
  } else {
    info("Click Copy URL and wait for clipboard");
    await waitForClipboardPromise(() => {
      return menuitemCopyUrl.click();
    }, expected);
  }

  info("Hide context menu");
}

function getBackgroundImageProperty(view, selector) {
  const isRuleView = view instanceof CssRuleView;
  if (isRuleView) {
    return getRuleViewProperty(view, selector, "background-image");
  }
  return getComputedViewProperty(view, "background-image");
}
