/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests ensure that capturing a site's screenshot to a canvas actually
 * works.
 */
function runTests() {
  // Create a tab with a red background.
  yield addTab("data:text/html,<body bgcolor=ff0000></body>");
  checkCurrentThumbnailColor(255, 0, 0, "we have a red thumbnail");

  // Load a page with a green background.
  yield navigateTo("data:text/html,<body bgcolor=00ff00></body>");
  checkCurrentThumbnailColor(0, 255, 0, "we have a green thumbnail");

  // Load a page with a blue background.
  yield navigateTo("data:text/html,<body bgcolor=0000ff></body>");
  checkCurrentThumbnailColor(0, 0, 255, "we have a blue thumbnail");
}

/**
 * Captures a thumbnail of the currently selected tab and checks the color of
 * the resulting canvas.
 * @param aRed The red component's intensity.
 * @param aGreen The green component's intensity.
 * @param aBlue The blue component's intensity.
 * @param aMessage The info message to print when checking the pixel color.
 */
function checkCurrentThumbnailColor(aRed, aGreen, aBlue, aMessage) {
  let tab = gBrowser.selectedTab;
  let cw = tab.linkedBrowser.contentWindow;

  let canvas = PageThumbs.capture(cw);
  let ctx = canvas.getContext("2d");

  checkCanvasColor(ctx, aRed, aGreen, aBlue, aMessage);
}
