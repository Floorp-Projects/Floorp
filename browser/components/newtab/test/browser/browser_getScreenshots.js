/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// a blue page
const TEST_URL =
  "https://example.com/browser/browser/components/newtab/test/browser/blue_page.html";
const XHTMLNS = "http://www.w3.org/1999/xhtml";

ChromeUtils.defineModuleGetter(
  this,
  "Screenshots",
  "resource://activity-stream/lib/Screenshots.jsm"
);

function get_pixels(stringOrObject, width, height) {
  return new Promise(resolve => {
    // get the pixels out of the screenshot that we just took
    let img = document.createElementNS(XHTMLNS, "img");
    let imgPath;

    if (typeof stringOrObject === "string") {
      Assert.ok(
        Services.prefs.getBoolPref(
          "browser.tabs.remote.separatePrivilegedContentProcess"
        ),
        "The privileged about content process should be enabled."
      );
      imgPath = stringOrObject;
      Assert.ok(
        imgPath.startsWith("moz-page-thumb://"),
        "Thumbnails should be retrieved using moz-page-thumb://"
      );
    } else {
      imgPath = URL.createObjectURL(stringOrObject.data);
    }

    img.setAttribute("src", imgPath);
    img.addEventListener(
      "load",
      () => {
        let canvas = document.createElementNS(XHTMLNS, "canvas");
        canvas.setAttribute("width", width);
        canvas.setAttribute("height", height);
        let ctx = canvas.getContext("2d");
        ctx.drawImage(img, 0, 0, width, height);
        const result = ctx.getImageData(0, 0, width, height).data;
        URL.revokeObjectURL(imgPath);
        resolve(result);
      },
      { once: true }
    );
  });
}

add_task(async function test_screenshot() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.pagethumbnails.capturing_disabled", false]],
  });

  // take a screenshot of a blue page and save it as a blob
  const screenshotAsObject = await Screenshots.getScreenshotForURL(TEST_URL);
  let pixels = await get_pixels(screenshotAsObject, 10, 10);
  let rgbaCount = { r: 0, g: 0, b: 0, a: 0 };
  while (pixels.length) {
    // break the pixels into arrays of 4 components [red, green, blue, alpha]
    let [r, g, b, a, ...rest] = pixels;
    pixels = rest;
    // count the number of each coloured pixels
    if (r === 255) {
      rgbaCount.r += 1;
    }
    if (g === 255) {
      rgbaCount.g += 1;
    }
    if (b === 255) {
      rgbaCount.b += 1;
    }
    if (a === 255) {
      rgbaCount.a += 1;
    }
  }

  // in the end, we should only have 100 blue pixels (10 x 10) with full opacity
  Assert.equal(rgbaCount.b, 100, "Has 100 blue pixels");
  Assert.equal(rgbaCount.a, 100, "Has full opacity");
  Assert.equal(rgbaCount.r, 0, "Does not have any red pixels");
  Assert.equal(rgbaCount.g, 0, "Does not have any green pixels");
});
