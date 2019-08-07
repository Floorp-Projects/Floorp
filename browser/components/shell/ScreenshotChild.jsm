"use strict";

const EXPORTED_SYMBOLS = ["ScreenshotChild"];

class ScreenshotChild extends JSWindowActorChild {
  receiveMessage(message) {
    if (message.name == "TakeScreenshot") {
      return this.takeScreenshot(message.data);
    }
    return null;
  }

  async takeScreenshot(params) {
    if (this.document.readyState != "complete") {
      await new Promise(resolve =>
        this.contentWindow.addEventListener("load", resolve, { once: true })
      );
    }

    let { fullWidth, fullHeight } = params;
    let { contentWindow } = this;

    let canvas = contentWindow.document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "html:canvas"
    );
    let context = canvas.getContext("2d");
    let width = contentWindow.innerWidth;
    let height = contentWindow.innerHeight;
    if (fullWidth) {
      width += contentWindow.scrollMaxX - contentWindow.scrollMinX;
    }
    if (fullHeight) {
      height += contentWindow.scrollMaxY - contentWindow.scrollMinY;
    }

    canvas.width = width;
    canvas.height = height;
    context.drawWindow(
      contentWindow,
      0,
      0,
      width,
      height,
      "rgb(255, 255, 255)"
    );

    return new Promise(resolve => canvas.toBlob(resolve));
  }
}
