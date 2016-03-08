/* globals XPCOMUtils, Services, PreviewProvider, registerCleanupFunction */
"use strict";

let Cu = Components.utils;
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PreviewProvider",
                                  "resource:///modules/PreviewProvider.jsm");

var oldEnabledPref = Services.prefs.getBoolPref("browser.pagethumbnails.capturing_disabled");
Services.prefs.setBoolPref("browser.pagethumbnails.capturing_disabled", false);

registerCleanupFunction(function() {
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeTab(gBrowser.tabs[1]);
  }
  Services.prefs.setBoolPref("browser.pagethumbnails.capturing_disabled", oldEnabledPref);
});

const TEST_URL = "https://example.com/browser/browser/components/newtab/tests/browser/blue_page.html";

function pixelsForDataURI(dataURI, options) {
  return new Promise(resolve => {
    if (!options) {
      options = {};
    }
    let {width, height} = options;
    if (!width) {
      width = 100;
    }
    if (!height) {
      height = 100;
    }

    let htmlns = "http://www.w3.org/1999/xhtml";
    let img = document.createElementNS(htmlns, "img");
    img.setAttribute("src", dataURI);

    img.addEventListener("load", function onLoad() {
      img.removeEventListener("load", onLoad, true);
      let canvas = document.createElementNS(htmlns, "canvas");
      canvas.setAttribute("width", width);
      canvas.setAttribute("height", height);
      let ctx = canvas.getContext("2d");
      ctx.drawImage(img, 0, 0, width, height);
      let result = ctx.getImageData(0, 0, width, height).data;
      resolve(result);
    });
  });
}

function* chunk_four(listData) {
  let index = 0;
  while (index < listData.length) {
    yield listData.slice(index, index + 5);
    index += 4;
  }
}

add_task(function* open_page() {
  let dataURI = yield PreviewProvider.getThumbnail(TEST_URL);
  let pixels = yield pixelsForDataURI(dataURI, {width: 10, height: 10});
  let rgbCount = {r: 0, g: 0, b: 0, a: 0};
  for (let [r, g, b, a] of chunk_four(pixels)) {
    if (r === 255) {
      rgbCount.r += 1;
    }
    if (g === 255) {
      rgbCount.g += 1;
    }
    if (b === 255) {
      rgbCount.b += 1;
    }
    if (a === 255) {
      rgbCount.a += 1;
    }
  }
  Assert.equal(`${rgbCount.r},${rgbCount.g},${rgbCount.b},${rgbCount.a}`,
      "0,0,100,100", "there should be 100 blue-only pixels at full opacity");
});

add_task(function* invalid_url() {
  try {
    yield PreviewProvider.getThumbnail("invalid:URL");
  } catch (err) {
    Assert.ok(true, "URL Failed");
  }
});
