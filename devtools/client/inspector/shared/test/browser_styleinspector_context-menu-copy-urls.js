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

add_task(function* () {
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

  yield addTab("data:text/html;charset=utf8," + encodeURIComponent(TEST_URI));

  yield startTest();
});

function* startTest() {
  info("Opening rule view");
  let {inspector, view} = yield openRuleView();

  info("Test valid background image URL in rule view");
  yield testCopyUrlToClipboard({view, inspector}, "data-uri",
    ".valid-background", TEST_DATA_URI);
  yield testCopyUrlToClipboard({view, inspector}, "url",
    ".valid-background", TEST_DATA_URI);

  info("Test invalid background image URL in rue view");
  yield testCopyUrlToClipboard({view, inspector}, "data-uri",
    ".invalid-background", ERROR_MESSAGE);
  yield testCopyUrlToClipboard({view, inspector}, "url",
    ".invalid-background", INVALID_IMAGE_URI);

  info("Opening computed view");
  view = selectComputedView(inspector);

  info("Test valid background image URL in computed view");
  yield testCopyUrlToClipboard({view, inspector}, "data-uri",
    ".valid-background", TEST_DATA_URI);
  yield testCopyUrlToClipboard({view, inspector}, "url",
    ".valid-background", TEST_DATA_URI);

  info("Test invalid background image URL in computed view");
  yield testCopyUrlToClipboard({view, inspector}, "data-uri",
    ".invalid-background", ERROR_MESSAGE);
  yield testCopyUrlToClipboard({view, inspector}, "url",
    ".invalid-background", INVALID_IMAGE_URI);
}

function* testCopyUrlToClipboard({view, inspector}, type, selector, expected) {
  info("Select node in inspector panel");
  yield selectNode(selector, inspector);

  info("Retrieve background-image link for selected node in current " +
       "styleinspector view");
  let property = getBackgroundImageProperty(view, selector);
  let imageLink = property.valueSpan.querySelector(".theme-link");
  ok(imageLink, "Background-image link element found");

  info("Simulate right click on the background-image URL");
  let allMenuItems = openStyleContextMenuAndGetAllItems(view, imageLink);
  let menuitemCopyUrl = allMenuItems.find(item => item.label ===
    STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copyUrl"));
  let menuitemCopyImageDataUrl = allMenuItems.find(item => item.label ===
    STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copyImageDataUrl"));

  info("Context menu is displayed");
  ok(menuitemCopyUrl.visible,
     "\"Copy URL\" menu entry is displayed");
  ok(menuitemCopyImageDataUrl.visible,
     "\"Copy Image Data-URL\" menu entry is displayed");

  if (type == "data-uri") {
    info("Click Copy Data URI and wait for clipboard");
    yield waitForClipboard(() => {
      return menuitemCopyImageDataUrl.click();
    }, expected);
  } else {
    info("Click Copy URL and wait for clipboard");
    yield waitForClipboard(() => {
      return menuitemCopyUrl.click();
    }, expected);
  }

  info("Hide context menu");
}

function getBackgroundImageProperty(view, selector) {
  let isRuleView = view instanceof CssRuleView;
  if (isRuleView) {
    return getRuleViewProperty(view, selector, "background-image");
  }
  return getComputedViewProperty(view, "background-image");
}
