/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["Layout"];

const { Assert } = ChromeUtils.import("resource://testing-common/Assert.jsm");
const { CommonUtils } = ChromeUtils.import(
  "chrome://mochitests/content/browser/accessible/tests/browser/Common.jsm"
);

const Layout = {
  /**
   * Zoom the given document.
   */
  zoomDocument(doc, zoom) {
    const bc = BrowsingContext.getFromWindow(doc.defaultView);
    // To mirror the behaviour of the UI, we set the zoom
    // value on the top level browsing context. This value automatically
    // propagates down to iframes.
    bc.top.fullZoom = zoom;
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
   * Assert.is() function checking the expected value is within the range.
   */
  isWithin(expected, got, within, msg) {
    if (Math.abs(got - expected) <= within) {
      Assert.ok(true, `${msg} - Got ${got}`);
    } else {
      Assert.ok(
        false,
        `${msg} - Got ${got}, expected ${expected} with error of ${within}`
      );
    }
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
      "X in CSS pixels is calculated correctly"
    );
    this.isWithin(
      y.value / dpr,
      yInCSS.value,
      1,
      "Y in CSS pixels is calculated correctly"
    );
    this.isWithin(
      width.value / dpr,
      widthInCSS.value,
      1,
      "Width in CSS pixels is calculated correctly"
    );
    this.isWithin(
      height.value / dpr,
      heightInCSS.value,
      1,
      "Height in CSS pixels is calculated correctly"
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

  CSSToDevicePixels(win, x, y, width, height) {
    const ratio = win.devicePixelRatio;

    // CSS pixels and ratio can be not integer. Device pixels are always integer.
    // Do our best and hope it works.
    return [
      Math.round(x * ratio),
      Math.round(y * ratio),
      Math.round(width * ratio),
      Math.round(height * ratio),
    ];
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

    return this.CSSToDevicePixels(
      elmWindow,
      x + elmWindow.mozInnerScreenX,
      y + elmWindow.mozInnerScreenY,
      width,
      height
    );
  },
};
