/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

let Cu = Components.utils;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// a blue page
const TEST_URL = "https://example.com/browser/browser/extensions/activity-stream/test/functional/mochitest/blue_page.html";
const XHTMLNS = "http://www.w3.org/1999/xhtml";

SpecialPowers.pushPrefEnv({set: [["browser.pagethumbnails.capturing_disabled", false]]});

XPCOMUtils.defineLazyModuleGetter(this, "Screenshots", "resource://activity-stream/lib/Screenshots.jsm");

function get_pixels_for_data_uri(dataURI, width, height) {
  return new Promise(resolve => {
    // get the pixels out of the screenshot that we just took
    let img = document.createElementNS(XHTMLNS, "img");
    img.setAttribute("src", dataURI);
    img.addEventListener("load", () => {
      let canvas = document.createElementNS(XHTMLNS, "canvas");
      canvas.setAttribute("width", width);
      canvas.setAttribute("height", height);
      let ctx = canvas.getContext("2d");
      ctx.drawImage(img, 0, 0, width, height);
      const result = ctx.getImageData(0, 0, width, height).data;
      resolve(result);
    }, {once: true});
  });
}

add_task(async function test_screenshot() {
  // take a screenshot of a blue page and save it as a data URI
  const screenshotAsDataURI = await Screenshots.getScreenshotForURL(TEST_URL);
  let pixels = await get_pixels_for_data_uri(screenshotAsDataURI, 10, 10);
  let rgbaCount = {r: 0, g: 0, b: 0, a: 0};
  while (pixels.length) {
    // break the pixels into arrays of 4 components [red, green, blue, alpha]
    let [r, g, b, a, ...rest] = pixels;
    pixels = rest;
    // count the number of each coloured pixels
    if (r === 255) { rgbaCount.r += 1; }
    if (g === 255) { rgbaCount.g += 1; }
    if (b === 255) { rgbaCount.b += 1; }
    if (a === 255) { rgbaCount.a += 1; }
  }

  // in the end, we should only have 100 blue pixels (10 x 10) with full opacity
  Assert.equal(rgbaCount.b, 100, "Has 100 blue pixels");
  Assert.equal(rgbaCount.a, 100, "Has full opacity");
  Assert.equal(rgbaCount.r, 0, "Does not have any red pixels");
  Assert.equal(rgbaCount.g, 0, "Does not have any green pixels");
});
