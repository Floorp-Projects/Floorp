/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { LocalizationHelper } = require("devtools/shared/l10n");

const CONTAINER_FLASHING_DURATION = 500;
const STRINGS_URI = "devtools/shared/locales/screenshot.properties";
const L10N = new LocalizationHelper(STRINGS_URI);

// These values are used to truncate the resulting image if the captured area is bigger.
// This is to avoid failing to produce a screenshot at all.
// It is recommended to keep these values in sync with the corresponding screenshots addon
// values in /browser/extensions/screenshots/selector/uicontrol.js
const MAX_IMAGE_WIDTH = 10000;
const MAX_IMAGE_HEIGHT = 10000;

/**
 * This function is called to simulate camera effects
 * @param {BrowsingContext} browsingContext: The browsing context associated with the
 *                          browser element we want to animate.
 */
function simulateCameraFlash(browsingContext) {
  // If there's no topFrameElement (it can happen if the screenshot is taken from the
  // browser toolbox), use the top chrome window document element.
  const node =
    browsingContext.topFrameElement ||
    browsingContext.topChromeWindow.document.documentElement;

  if (!node) {
    console.error(
      "Can't find an element to play the camera flash animation on for the following browsing context:",
      browsingContext
    );
    return;
  }

  // Don't take a screenshot if the user prefers reduced motion.
  if (node.ownerGlobal.matchMedia("(prefers-reduced-motion)").matches) {
    return;
  }

  node.animate([{ opacity: 0 }, { opacity: 1 }], {
    duration: CONTAINER_FLASHING_DURATION,
  });
}

/**
 * Take a screenshot of a browser element given its browsingContext.
 *
 * @param {Object} args
 * @param {Number} args.delay: Number of seconds to wait before taking the screenshot
 * @param {Object|null} args.rect: Object with left, top, width and height properties
 *                      representing the rect **inside the browser element** that should
 *                      be rendered. If null, the current viewport of the element will be rendered.
 * @param {Boolean} args.fullpage: Should the screenshot be the height of the whole page
 * @param {String} args.filename: Expected filename for the screenshot
 * @param {Number} args.snapshotScale: Scale that will be used by `drawSnapshot` to take the screenshot.
 *                 ⚠️ Note that the scale might be decreased if the resulting image would
 *                 be too big to draw safely. A warning message will be returned if that's
 *                 the case.
 * @param {Number} args.fileScale: Scale of the exported file. Defaults to args.snapshotScale.
 * @param {Boolean} args.disableFlash: Set to true to disable the flash animation when the
 *                  screenshot is taken.
 * @param {BrowsingContext} browsingContext
 * @returns {Object} object with the following properties:
 *          - data {String}: The dataURL representing the screenshot
 *          - height {Number}: Height of the resulting screenshot
 *          - width {Number}: Width of the resulting screenshot
 *          - filename {String}: Filename of the resulting screenshot
 *          - messages {Array<Object{text, level}>}: An array of object representing the
 *            different messages and their level that should be displayed to the user.
 */
async function captureScreenshot(args, browsingContext) {
  const messages = [];

  let filename = getFilename(args.filename);

  if (args.fullpage) {
    filename = filename.replace(".png", "-fullpage.png");
  }

  let { left, top, width, height } = args.rect || {};

  // Truncate the width and height if necessary.
  if (width && (width > MAX_IMAGE_WIDTH || height > MAX_IMAGE_HEIGHT)) {
    width = Math.min(width, MAX_IMAGE_WIDTH);
    height = Math.min(height, MAX_IMAGE_HEIGHT);
    messages.push({
      level: "warn",
      text: L10N.getFormatStr("screenshotTruncationWarning", width, height),
    });
  }

  let rect = null;
  if (args.rect) {
    rect = new globalThis.DOMRect(
      Math.round(left),
      Math.round(top),
      Math.round(width),
      Math.round(height)
    );
  }

  const document = browsingContext.topChromeWindow.document;
  const canvas = document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "canvas"
  );

  const drawToCanvas = async actualRatio => {
    // Even after decreasing width, height and ratio, there may still be cases where the
    // hardware fails at creating the image. Let's catch this so we can at least show an
    // error message to the user.
    try {
      const snapshot = await browsingContext.currentWindowGlobal.drawSnapshot(
        rect,
        actualRatio,
        "rgb(255,255,255)"
      );

      const fileScale = args.fileScale || actualRatio;
      const renderingWidth = (snapshot.width / actualRatio) * fileScale;
      const renderingHeight = (snapshot.height / actualRatio) * fileScale;
      canvas.width = renderingWidth;
      canvas.height = renderingHeight;
      width = renderingWidth;
      height = renderingHeight;
      const ctx = canvas.getContext("2d");
      ctx.drawImage(snapshot, 0, 0, renderingWidth, renderingHeight);

      // Bug 1574935 - Huge dimensions can trigger an OOM because multiple copies
      // of the bitmap will exist in memory. Force the removal of the snapshot
      // because it is no longer needed.
      snapshot.close();

      return canvas.toDataURL("image/png", "");
    } catch (e) {
      return null;
    }
  };

  const ratio = args.snapshotScale;
  let data = await drawToCanvas(ratio);
  if (!data && ratio > 1.0) {
    // If the user provided DPR or the window.devicePixelRatio was higher than 1,
    // try again with a reduced ratio.
    messages.push({
      level: "warn",
      text: L10N.getStr("screenshotDPRDecreasedWarning"),
    });
    data = await drawToCanvas(1.0);
  }
  if (!data) {
    messages.push({
      level: "error",
      text: L10N.getStr("screenshotRenderingError"),
    });
  }

  if (data && args.disableFlash !== true) {
    simulateCameraFlash(browsingContext);
  }

  return {
    data,
    height,
    width,
    filename,
    messages,
  };
}

exports.captureScreenshot = captureScreenshot;

/**
 * We may have a filename specified in args, or we might have to generate
 * one.
 */
function getFilename(defaultName) {
  // Create a name for the file if not present
  if (defaultName) {
    return defaultName;
  }

  const date = new Date();
  const monthString = (date.getMonth() + 1).toString().padStart(2, "0");
  const dayString = date
    .getDate()
    .toString()
    .padStart(2, "0");
  const dateString = `${date.getFullYear()}-${monthString}-${dayString}`;

  const timeString = date
    .toTimeString()
    .replace(/:/g, ".")
    .split(" ")[0];

  return (
    L10N.getFormatStr("screenshotGeneratedFilename", dateString, timeString) +
    ".png"
  );
}
