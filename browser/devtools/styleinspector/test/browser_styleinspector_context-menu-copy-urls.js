/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* Tests both Copy URL and Copy Data URL context menu items */

const PROPERTIES_URL = "chrome://global/locale/devtools/styleinspector.properties";
const TEST_DATA_URI = "data:image/gif;base64,R0lGODlhAQABAIAAAP///wAAACwAAAAAAQABAAACAkQBADs=";

// invalid URL still needs to be reachable otherwise getImageDataUrl will timeout.
// Reusing the properties bundle URL as a workaround
const INVALID_IMAGE_URI = PROPERTIES_URL;

const ERROR_MESSAGE = Services.strings
  .createBundle(PROPERTIES_URL)
  .GetStringFromName("styleinspector.copyImageDataUrlError");

add_task(function*() {
  const PAGE_CONTENT = [
    "<style type=\"text/css\">",
    "  .valid-background {",
    "    background-image: url(" + TEST_DATA_URI + ");",
    "  }",
    "  .invalid-background {",
    "    background-image: url(" + INVALID_IMAGE_URI + ");",
    "  }",
    "</style>",
    "<div class=\"valid-background\">Valid background image</div>",
    "<div class=\"invalid-background\">Invalid background image</div>"
  ].join("\n");

  yield addTab("data:text/html;charset=utf8," + encodeURIComponent(PAGE_CONTENT));

  yield startTest();
});

function* startTest() {
  info("Opening rule view");
  let ruleViewData = yield openRuleView();

  info("Test valid background image URL in rule view");
  yield testCopyUrlToClipboard(ruleViewData, "data-uri", ".valid-background", TEST_DATA_URI);
  yield testCopyUrlToClipboard(ruleViewData, "url", ".valid-background", TEST_DATA_URI);
  info("Test invalid background image URL in rue view");
  yield testCopyUrlToClipboard(ruleViewData, "data-uri", ".invalid-background", ERROR_MESSAGE);
  yield testCopyUrlToClipboard(ruleViewData, "url", ".invalid-background", PROPERTIES_URL);

  info("Opening computed view");
  let computedViewData = yield openComputedView();

  info("Test valid background image URL in computed view");
  yield testCopyUrlToClipboard(computedViewData, "data-uri", ".valid-background", TEST_DATA_URI);
  yield testCopyUrlToClipboard(computedViewData, "url", ".valid-background", TEST_DATA_URI);
  info("Test invalid background image URL in computed view");
  yield testCopyUrlToClipboard(computedViewData, "data-uri", ".invalid-background", ERROR_MESSAGE);
  yield testCopyUrlToClipboard(computedViewData, "url", ".invalid-background", PROPERTIES_URL);
}

function* testCopyUrlToClipboard({view, inspector}, type, selector, expected) {
  info("Select node in inspector panel");
  yield selectNode(selector, inspector);

  info("Retrieve background-image link for selected node in current styleinspector view");
  let property = getBackgroundImageProperty(view, selector);
  let imageLink = property.valueSpan.querySelector(".theme-link");
  ok(imageLink, "Background-image link element found");

  info("Simulate right click on the background-image URL");
  let popup = once(view._contextmenu, "popupshown");

  // Cannot rely on synthesizeMouseAtCenter here. The image URL can be displayed on several lines.
  // A click simulated at the exact center may click between the lines and miss the target
  // Instead, using the top-left corner of first client rect, with an offset of 2 pixels.
  let rect = imageLink.getClientRects()[0];
  let x = rect.left + 2;
  let y = rect.top + 2;

  EventUtils.synthesizeMouseAtPoint(x, y, {button: 2, type: "contextmenu"}, getViewWindow(view));
  yield popup;

  info("Context menu is displayed");
  ok(!view.menuitemCopyImageDataUrl.hidden, "\"Copy Image Data-URL\" menu entry is displayed");

  if (type == "data-uri") {
    info("Click Copy Data URI and wait for clipboard");
    yield waitForClipboard(() => view.menuitemCopyImageDataUrl.click(), expected);
  } else {
    info("Click Copy URL and wait for clipboard");
    yield waitForClipboard(() => view.menuitemCopyUrl.click(), expected);
  }

  info("Hide context menu");
  view._contextmenu.hidePopup();
}

function getBackgroundImageProperty(view, selector) {
  let isRuleView = view instanceof CssRuleView;
  if (isRuleView) {
    return getRuleViewProperty(view, selector, "background-image");
  } else {
    return getComputedViewProperty(view, "background-image");
  }
}

/**
 * Function that returns the window for a given view.
 */
function getViewWindow(view) {
  let viewDocument = view.styleDocument ? view.styleDocument : view.doc;
  return viewDocument.defaultView;
}
