/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const { Cu, Cc, Ci } = require("chrome");
const Services = require("Services");
const { LocalizationHelper } = require("devtools/shared/l10n");

loader.lazyRequireGetter(this, "getRect", "devtools/shared/layout/utils", true);

const CONTAINER_FLASHING_DURATION = 500;
const STRINGS_URI = "devtools/shared/locales/screenshot.properties";
const L10N = new LocalizationHelper(STRINGS_URI);

// These values are used to truncate the resulting image if the captured area is bigger.
// This is to avoid failing to produce a screenshot at all.
// It is recommended to keep these values in sync with the corresponding screenshots addon
// values in /browser/extensions/screenshots/build/buildSettings.js
const MAX_IMAGE_WIDTH = 10000;
const MAX_IMAGE_HEIGHT = 10000;

/**
 * This function is called to simulate camera effects
 * @param object document
 *        The target document.
 */
function simulateCameraFlash(document) {
  const window = document.defaultView;
  const frames = Cu.cloneInto({ opacity: [0, 1] }, window);
  document.documentElement.animate(frames, CONTAINER_FLASHING_DURATION);
}

/**
 * This function simply handles the --delay argument before calling
 * createScreenshotData
 */
function captureScreenshot(args, document) {
  if (args.help) {
    return null;
  }
  if (args.delay > 0) {
    return new Promise((resolve, reject) => {
      document.defaultView.setTimeout(() => {
        createScreenshotDataURL(document, args).then(resolve, reject);
      }, args.delay * 1000);
    });
  }
  return createScreenshotDataURL(document, args);
}

exports.captureScreenshot = captureScreenshot;

/**
 * This does the dirty work of creating a base64 string out of an
 * area of the browser window
 */
function createScreenshotDataURL(document, args) {
  let window = document.defaultView;
  let left = 0;
  let top = 0;
  let width;
  let height;
  const currentX = window.scrollX;
  const currentY = window.scrollY;

  let filename = getFilename(args.filename);

  if (args.fullpage) {
    // Bug 961832: Screenshot shows fixed position element in wrong
    // position if we don't scroll to top
    window.scrollTo(0, 0);
    width = window.innerWidth + window.scrollMaxX - window.scrollMinX;
    height = window.innerHeight + window.scrollMaxY - window.scrollMinY;
    filename = filename.replace(".png", "-fullpage.png");
  } else if (args.rawNode) {
    window = args.rawNode.ownerDocument.defaultView;
    ({ top, left, width, height } = getRect(window, args.rawNode, window));
  } else if (args.selector) {
    const node = window.document.querySelector(args.selector);
    ({ top, left, width, height } = getRect(window, node, window));
  } else {
    left = window.scrollX;
    top = window.scrollY;
    width = window.innerWidth;
    height = window.innerHeight;
  }

  // Only adjust for scrollbars when considering the full window
  if (args.fullpage) {
    const winUtils = window.windowUtils;
    const scrollbarHeight = {};
    const scrollbarWidth = {};
    winUtils.getScrollbarSize(false, scrollbarWidth, scrollbarHeight);
    width -= scrollbarWidth.value;
    height -= scrollbarHeight.value;
  }

  let ratio;
  if (args.fullpage) {
    // Always take fullpage screenshots at dpr=1 to avoid failures.
    ratio = 1;
    // Warn the user if they had provided a higher dpr or have a high resolution monitor.
    if ((args.dpr && args.dpr > 1) || window.devicePixelRatio > 1) {
      logWarningInPage(L10N.getStr("screenshotDPRDecreasedWarning"), window);
    }
  } else {
    ratio = args.dpr ? args.dpr : window.devicePixelRatio;
  }

  // Truncate the width and height if necessary.
  if (width > MAX_IMAGE_WIDTH || height > MAX_IMAGE_HEIGHT) {
    width = Math.min(width, MAX_IMAGE_WIDTH);
    height = Math.min(height, MAX_IMAGE_HEIGHT);
    logWarningInPage(
      L10N.getFormatStr("screenshotTruncationWarning", width, height),
      window
    );
  }

  const canvas = document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "canvas"
  );
  const ctx = canvas.getContext("2d");

  // Even after decreasing width, height and ratio, there may still be cases where the
  // hardware fails at creating the image. Let's catch this so we can at least show an
  // error message to the user.
  let data = null;
  try {
    canvas.width = width * ratio;
    canvas.height = height * ratio;
    ctx.scale(ratio, ratio);
    ctx.drawWindow(window, left, top, width, height, "#fff");
    data = canvas.toDataURL("image/png", "");
  } catch (e) {
    logErrorInPage(L10N.getStr("screenshotRenderingError"), window);
  }

  // See comment above on bug 961832
  if (args.fullpage) {
    window.scrollTo(currentX, currentY);
  }

  if (data) {
    simulateCameraFlash(document);
  }

  return Promise.resolve({
    destinations: [],
    data,
    height,
    width,
    filename,
  });
}

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
  let dateString =
    date.getFullYear() + "-" + (date.getMonth() + 1) + "-" + date.getDate();
  dateString = dateString
    .split("-")
    .map(function(part) {
      if (part.length == 1) {
        part = "0" + part;
      }
      return part;
    })
    .join("-");

  const timeString = date
    .toTimeString()
    .replace(/:/g, ".")
    .split(" ")[0];
  return (
    L10N.getFormatStr("screenshotGeneratedFilename", dateString, timeString) +
    ".png"
  );
}

function logInPage(text, flags, window) {
  const scriptError = Cc["@mozilla.org/scripterror;1"].createInstance(
    Ci.nsIScriptError
  );
  scriptError.initWithWindowID(
    text,
    null,
    null,
    0,
    0,
    flags,
    "screenshot",
    window.windowUtils.currentInnerWindowID
  );
  Services.console.logMessage(scriptError);
}

const logErrorInPage = (text, window) => logInPage(text, 0, window);
const logWarningInPage = (text, window) => logInPage(text, 1, window);
