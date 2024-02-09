/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  getBestRectForElement,
  getElementFromPoint,
} from "chrome://browser/content/screenshots/overlayHelpers.mjs";

/**
 * This class is used to get the dimensions of hovered elements within iframes.
 * The main content process cannot get the dimensions of elements within
 * iframes so a message will be send to this actor to get the dimensions of the
 * element for a given point inside the iframe.
 */
export class ScreenshotsHelperChild extends JSWindowActorChild {
  receiveMessage(message) {
    if (message.name === "ScreenshotsHelper:GetElementRectFromPoint") {
      return this.getBestElementRectFromPoint(message.data);
    }
    return null;
  }

  async getBestElementRectFromPoint(data) {
    let { x, y } = data;

    x -= this.contentWindow.mozInnerScreenX;
    y -= this.contentWindow.mozInnerScreenY;

    let { ele, rect } = await getElementFromPoint(x, y, this.document);

    if (!rect) {
      rect = getBestRectForElement(ele, this.document);
    }

    if (rect) {
      rect = {
        left: rect.left + this.contentWindow.mozInnerScreenX,
        right: rect.right + this.contentWindow.mozInnerScreenX,
        top: rect.top + this.contentWindow.mozInnerScreenY,
        bottom: rect.bottom + this.contentWindow.mozInnerScreenY,
      };
    }

    return rect;
  }
}
