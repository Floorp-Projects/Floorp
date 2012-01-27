/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests ensure that saving a thumbnail to the cache works. They also
 * retrieve the thumbnail and display it using an <img> element to compare
 * its pixel colors.
 */
function runTests() {
  // Create a new tab with a red background.
  yield addTab("data:text/html,<body bgcolor=ff0000></body>");
  let cw = gBrowser.selectedTab.linkedBrowser.contentWindow;

  // Capture a thumbnail for the tab.
  let canvas = PageThumbs.capture(cw);

  // Store the tab into the thumbnail cache.
  yield PageThumbs.store("key", canvas, next);

  let {width, height} = canvas;
  let thumb = PageThumbs.getThumbnailURL("key", width, height);

  // Create a new tab with an image displaying the previously stored thumbnail.
  yield addTab("data:text/html,<img src='" + thumb + "'/>" + 
               "<canvas width=" + width + " height=" + height + "/>");

  cw = gBrowser.selectedTab.linkedBrowser.contentWindow;
  let [img, canvas] = cw.document.querySelectorAll("img, canvas");

  // Draw the image to a canvas and compare the pixel color values.
  let ctx = canvas.getContext("2d");
  ctx.drawImage(img, 0, 0, width, height);
  checkCanvasColor(ctx, 255, 0, 0, "we have a red image and canvas");
}
