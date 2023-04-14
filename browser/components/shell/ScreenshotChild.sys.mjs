/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class ScreenshotChild extends JSWindowActorChild {
  receiveMessage(message) {
    if (message.name == "GetDimensions") {
      return this.getDimensions();
    }
    return null;
  }

  async getDimensions() {
    if (this.document.readyState != "complete") {
      await new Promise(resolve =>
        this.contentWindow.addEventListener("load", resolve, { once: true })
      );
    }

    let { contentWindow } = this;

    return {
      innerWidth: contentWindow.innerWidth,
      innerHeight: contentWindow.innerHeight,
      scrollMinX: contentWindow.scrollMinX,
      scrollMaxX: contentWindow.scrollMaxX,
      scrollMinY: contentWindow.scrollMinY,
      scrollMaxY: contentWindow.scrollMaxY,
    };
  }
}
