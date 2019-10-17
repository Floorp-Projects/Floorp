/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["Layout"];

const { CommonUtils } = ChromeUtils.import(
  "chrome://mochitests/content/browser/accessible/tests/browser/Common.jsm"
);

function CSSToDevicePixels(win, x, y, width, height) {
  const winUtil = win.windowUtils;
  const ratio = winUtil.screenPixelsPerCSSPixel;

  // CSS pixels and ratio can be not integer. Device pixels are always integer.
  // Do our best and hope it works.
  return [
    Math.round(x * ratio),
    Math.round(y * ratio),
    Math.round(width * ratio),
    Math.round(height * ratio),
  ];
}

const Layout = {
  /**
   * Zoom the given document.
   */
  zoomDocument(doc, zoom) {
    const docShell = doc.defaultView.docShell;
    const docViewer = docShell.contentViewer;
    docViewer.fullZoom = zoom;
  },

  /**
   * Set the relative resolution of this document. This is what apz does.
   * On non-mobile platforms you won't see a visible change.
   */
  setResolution(doc, zoom) {
    const windowUtils = doc.defaultView.windowUtils;
    windowUtils.setResolutionAndScaleTo(zoom);
  },

  /**
   * Test text position at the given offset.
   */
  testTextPos(id, offset, point, coordOrigin) {
    const [expectedX, expectedY] = point;

    const xObj = {};
    const yObj = {};
    const hyperText = CommonUtils.getAccessible(id, [Ci.nsIAccessibleText]);
    hyperText.getCharacterExtents(offset, xObj, yObj, {}, {}, coordOrigin);

    is(
      xObj.value,
      expectedX,
      `Wrong x coordinate at offset ${offset} for ${CommonUtils.prettyName(id)}`
    );
    ok(
      yObj.value - expectedY < 2 && expectedY - yObj.value < 2,
      `Wrong y coordinate at offset ${offset} for ${CommonUtils.prettyName(
        id
      )} - got ${
        yObj.value
      }, expected ${expectedY}. The difference doesn't exceed 1.`
    );
  },

  /**
   * is() function checking the expected value is within the range.
   */
  isWithin(expected, got, within, msg) {
    if (Math.abs(got - expected) <= within) {
      ok(true, `${msg} - Got ${got}`);
    } else {
      ok(
        false,
        `${msg} - Got ${got}, expected ${expected} with error of ${within}`
      );
    }
  },

  /**
   * Test text bounds that is enclosed betwene the given offsets.
   */
  testTextBounds(id, startOffset, endOffset, rect, coordOrigin) {
    const [expectedX, expectedY, expectedWidth, expectedHeight] = rect;

    const xObj = {};
    const yObj = {};
    const widthObj = {};
    const heightObj = {};
    const hyperText = CommonUtils.getAccessible(id, [Ci.nsIAccessibleText]);
    hyperText.getRangeExtents(
      startOffset,
      endOffset,
      xObj,
      yObj,
      widthObj,
      heightObj,
      coordOrigin
    );

    // x
    is(
      xObj.value,
      expectedX,
      `Wrong x coordinate of text between offsets (${startOffset}, ${endOffset}) for ${CommonUtils.prettyName(
        id
      )}`
    );

    // y
    this.isWithin(
      yObj.value,
      expectedY,
      1,
      `y coord of text between offsets (${startOffset}, ${endOffset}) for ${CommonUtils.prettyName(
        id
      )}`
    );

    // Width
    const msg = `Wrong width of text between offsets (${startOffset}, ${endOffset}) for ${CommonUtils.prettyName(
      id
    )}`;
    if (widthObj.value == expectedWidth) {
      ok(true, msg);
    } else {
      todo(false, msg);
    } // fails on some windows machines

    // Height
    this.isWithin(
      heightObj.value,
      expectedHeight,
      1,
      `height of text between offsets (${startOffset}, ${endOffset}) for ${CommonUtils.prettyName(
        id
      )}`
    );
  },

  /**
   * Return the accessible coordinates relative to the screen in device pixels.
   */
  getPos(id) {
    const accessible = CommonUtils.getAccessible(id);
    const x = {};
    const y = {};
    accessible.getBounds(x, y, {}, {});

    return [x.value, y.value];
  },

  /**
   * Return the accessible coordinates and size relative to the screen in device
   * pixels. This methods also retrieves coordinates in CSS pixels and ensures that they
   * match Dev pixels with a given device pixel ratio.
   */
  getBounds(id, dpr) {
    const accessible = CommonUtils.getAccessible(id);
    const x = {};
    const y = {};
    const width = {};
    const height = {};
    const xInCSS = {};
    const yInCSS = {};
    const widthInCSS = {};
    const heightInCSS = {};
    accessible.getBounds(x, y, width, height);
    accessible.getBoundsInCSSPixels(xInCSS, yInCSS, widthInCSS, heightInCSS);

    this.isWithin(
      x.value / dpr,
      xInCSS.value,
      1,
      "Heights in CSS pixels is calculated correctly"
    );
    this.isWithin(
      y.value / dpr,
      yInCSS.value,
      1,
      "Heights in CSS pixels is calculated correctly"
    );
    this.isWithin(
      width.value / dpr,
      widthInCSS.value,
      1,
      "Heights in CSS pixels is calculated correctly"
    );
    this.isWithin(
      height.value / dpr,
      heightInCSS.value,
      1,
      "Heights in CSS pixels is calculated correctly"
    );

    return [x.value, y.value, width.value, height.value];
  },

  getRangeExtents(id, startOffset, endOffset, coordOrigin) {
    const hyperText = CommonUtils.getAccessible(id, [Ci.nsIAccessibleText]);
    const x = {};
    const y = {};
    const width = {};
    const height = {};
    hyperText.getRangeExtents(
      startOffset,
      endOffset,
      x,
      y,
      width,
      height,
      coordOrigin
    );

    return [x.value, y.value, width.value, height.value];
  },

  /**
   * Return DOM node coordinates relative the screen and its size in device
   * pixels.
   */
  getBoundsForDOMElm(id, doc) {
    let x = 0;
    let y = 0;
    let width = 0;
    let height = 0;

    const elm = CommonUtils.getNode(id, doc);
    const elmWindow = elm.ownerGlobal;
    if (elm.localName == "area") {
      const mapName = elm.parentNode.getAttribute("name");
      const selector = `[usemap="#${mapName}"]`;
      const img = elm.ownerDocument.querySelector(selector);

      const areaCoords = elm.coords.split(",");
      const areaX = parseInt(areaCoords[0], 10);
      const areaY = parseInt(areaCoords[1], 10);
      const areaWidth = parseInt(areaCoords[2], 10) - areaX;
      const areaHeight = parseInt(areaCoords[3], 10) - areaY;

      const rect = img.getBoundingClientRect();
      x = rect.left + areaX;
      y = rect.top + areaY;
      width = areaWidth;
      height = areaHeight;
    } else {
      const rect = elm.getBoundingClientRect();
      x = rect.left;
      y = rect.top;
      width = rect.width;
      height = rect.height;
    }

    return CSSToDevicePixels(
      elmWindow,
      x + elmWindow.mozInnerScreenX,
      y + elmWindow.mozInnerScreenY,
      width,
      height
    );
  },
};
