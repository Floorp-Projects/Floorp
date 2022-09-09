/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci } = require("chrome");
const { LocalizationHelper } = require("devtools/shared/l10n");

loader.lazyImporter(this, "Downloads", "resource://gre/modules/Downloads.jsm");
loader.lazyImporter(this, "FileUtils", "resource://gre/modules/FileUtils.jsm");
loader.lazyImporter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

const STRINGS_URI = "devtools/shared/locales/screenshot.properties";
const L10N = new LocalizationHelper(STRINGS_URI);

/**
 * Take a screenshot of a browser element matching the passed target and save it to a file
 * or the clipboard.
 *
 * @param {TargetFront} targetFront: The targetFront of the frame we want to take a screenshot of.
 * @param {Window} window: The DevTools Client window.
 * @param {Object} args
 * @param {Boolean} args.fullpage: Should the screenshot be the height of the whole page
 * @param {String} args.filename: Expected filename for the screenshot
 * @param {Boolean} args.clipboard: Whether or not the screenshot should be saved to the clipboard.
 * @param {Number} args.dpr: Scale of the screenshot. Defaults to the window `devicePixelRatio`.
 *                           ⚠️ Note that the scale might be decreased if the resulting
 *                           image would be too big to draw safely. Warning will be emitted
 *                           to the console if that's the case.
 * @param {Number} args.delay: Number of seconds to wait before taking the screenshot
 * @param {Boolean} args.help: Set to true to receive a message with the screenshot command
 *                             documentation.
 * @param {Boolean} args.disableFlash: Set to true to disable the flash animation when the
 *                  screenshot is taken.
 * @param {Boolean} args.ignoreDprForFileScale: Set to true to if the resulting screenshot
 *                  file size shouldn't be impacted by the dpr. Note that the dpr will still
 *                  be taken into account when taking the screenshot, only the size of the
 *                  file will be different.
 * @returns {Array<Object{text, level}>} An array of object representing the different
 *          messages emitted throught the process, that should be displayed to the user.
 */
async function captureAndSaveScreenshot(targetFront, window, args = {}) {
  if (args.help) {
    // Wrap message in an array so that the return value is consistant.
    return [{ text: getFormattedHelpData() }];
  }

  const captureResponse = await captureScreenshot(targetFront, args);

  if (captureResponse.error) {
    return captureResponse.messages || [];
  }

  const saveMessages = await saveScreenshot(window, args, captureResponse);
  return (captureResponse.messages || []).concat(saveMessages);
}

/**
 * Take a screenshot of a browser element matching the passed target
 * @param {TargetFront} targetFront: The targetFront of the frame we want to take a screenshot of.
 * @param {Object} args: See args param in captureAndSaveScreenshot
 */
async function captureScreenshot(targetFront, args) {
  // @backward-compat { version 87 } The screenshot-content actor was introduced in 87,
  // so we can always use it once 87 reaches release.
  const supportsContentScreenshot = targetFront.hasActor("screenshotContent");
  if (!supportsContentScreenshot) {
    const screenshotFront = await targetFront.getFront("screenshot");
    return screenshotFront.capture(args);
  }

  if (args.delay > 0) {
    await new Promise(res => setTimeout(res, args.delay * 1000));
  }

  const screenshotContentFront = await targetFront.getFront(
    "screenshot-content"
  );

  // Call the content-process on the server to retrieve informations that will be needed
  // by the parent process.
  const {
    rect,
    windowDpr,
    windowZoom,
    messages,
    error,
  } = await screenshotContentFront.prepareCapture(args);

  if (error) {
    return { error, messages };
  }

  if (rect) {
    args.rect = rect;
  }

  args.dpr ||= windowDpr;

  args.snapshotScale = args.dpr * windowZoom;
  if (args.ignoreDprForFileScale) {
    args.fileScale = windowZoom;
  }

  args.browsingContextID = targetFront.browsingContextID;

  // We can now call the parent process which will take the screenshot via
  // the drawSnapshot API
  const rootFront = targetFront.client.mainRoot;
  const parentProcessScreenshotFront = await rootFront.getFront("screenshot");
  const captureResponse = await parentProcessScreenshotFront.capture(args);

  return {
    ...captureResponse,
    messages: (messages || []).concat(captureResponse.messages || []),
  };
}

const screenshotDescription = L10N.getStr("screenshotDesc");
const screenshotGroupOptions = L10N.getStr("screenshotGroupOptions");
const screenshotCommandParams = [
  {
    name: "clipboard",
    type: "boolean",
    description: L10N.getStr("screenshotClipboardDesc"),
    manual: L10N.getStr("screenshotClipboardManual"),
  },
  {
    name: "delay",
    type: "number",
    description: L10N.getStr("screenshotDelayDesc"),
    manual: L10N.getStr("screenshotDelayManual"),
  },
  {
    name: "dpr",
    type: "number",
    description: L10N.getStr("screenshotDPRDesc"),
    manual: L10N.getStr("screenshotDPRManual"),
  },
  {
    name: "fullpage",
    type: "boolean",
    description: L10N.getStr("screenshotFullPageDesc"),
    manual: L10N.getStr("screenshotFullPageManual"),
  },
  {
    name: "selector",
    type: "string",
    description: L10N.getStr("inspectNodeDesc"),
    manual: L10N.getStr("inspectNodeManual"),
  },
  {
    name: "file",
    type: "boolean",
    description: L10N.getStr("screenshotFileDesc"),
    manual: L10N.getStr("screenshotFileManual"),
  },
  {
    name: "filename",
    type: "string",
    description: L10N.getStr("screenshotFilenameDesc"),
    manual: L10N.getStr("screenshotFilenameManual"),
  },
];

/**
 * Creates a string from an object for use when screenshot is passed the `--help` argument
 *
 * @param object param
 *        The param object to be formatted.
 * @return string
 *         The formatted information from the param object as a string
 */
function formatHelpField(param) {
  const padding = " ".repeat(5);
  return Object.entries(param)
    .map(([key, value]) => {
      if (key === "name") {
        const name = `${padding}--${value}`;
        return name;
      }
      return `${padding.repeat(2)}${key}: ${value}`;
    })
    .join("\n");
}

/**
 * Creates a string response from the screenshot options for use when
 * screenshot is passed the `--help` argument
 *
 * @return string
 *         The formatted information from the param object as a string
 */
function getFormattedHelpData() {
  const formattedParams = screenshotCommandParams
    .map(formatHelpField)
    .join("\n\n");

  return `${screenshotDescription}\n${screenshotGroupOptions}\n\n${formattedParams}`;
}

/**
 * Main entry point in this file; Takes the original arguments that `:screenshot` was
 * called with and the image value from the server, and uses the client window to add
 * and audio effect.
 *
 * @param object window
 *        The DevTools Client window.
 *
 * @param object args
 *        The original args with which the screenshot
 *        was called.
 * @param object value
 *        an object with a image value and file name
 *
 * @return string[]
 *         Response messages from processing the screenshot
 */
function saveScreenshot(window, args = {}, value) {
  // @backward-compat { version 87 } This is still needed by the console when connecting
  // to an older server. Once 87 is in release, we can remove this whole block since we
  // already handle args.help in captureScreenshotAndSave.
  if (args.help) {
    // Wrap message in an array so that the return value is consistant.
    return [{ text: getFormattedHelpData() }];
  }

  // Guard against missing image data.
  if (!value.data) {
    return [];
  }

  simulateCameraShutter(window);
  return save(window, args, value);
}

/**
 * This function is called to simulate camera effects
 *
 * @param object document
 *        The DevTools Client document.
 */
function simulateCameraShutter(window) {
  if (Services.prefs.getBoolPref("devtools.screenshot.audio.enabled")) {
    const audioCamera = new window.Audio(
      "resource://devtools/client/themes/audio/shutter.wav"
    );
    audioCamera.play();
  }
}

/**
 * Save the captured screenshot to one of several destinations.
 *
 * @param object window
 *        The DevTools Client window.
 *
 * @param object args
 *        The original args with which the screenshot was called.
 *
 * @param object image
 *        The image object that was sent from the server.
 *
 *
 * @return string[]
 *         Response messages from processing the screenshot.
 */
async function save(window, args, image) {
  const fileNeeded = args.filename || !args.clipboard || args.file;
  const results = [];

  if (args.clipboard) {
    const result = saveToClipboard(image.data);
    results.push(result);
  }

  if (fileNeeded) {
    const result = await saveToFile(window, image);
    results.push(result);
  }
  return results;
}

/**
 * Save the image data to the clipboard. This returns a promise, so it can
 * be treated exactly like file processing.
 *
 * @param string base64URI
 *        The image data encoded in a base64 URI that was sent from the server.
 *
 * @return string
 *         Response message from processing the screenshot.
 */
function saveToClipboard(base64URI) {
  try {
    const imageTools = Cc["@mozilla.org/image/tools;1"].getService(
      Ci.imgITools
    );

    const base64Data = base64URI.replace("data:image/png;base64,", "");

    const image = atob(base64Data);
    const img = imageTools.decodeImageFromBuffer(
      image,
      image.length,
      "image/png"
    );

    const transferable = Cc[
      "@mozilla.org/widget/transferable;1"
    ].createInstance(Ci.nsITransferable);
    transferable.init(null);
    transferable.addDataFlavor("image/png");
    transferable.setTransferData("image/png", img);

    Services.clipboard.setData(
      transferable,
      null,
      Services.clipboard.kGlobalClipboard
    );
    return { text: L10N.getStr("screenshotCopied") };
  } catch (ex) {
    console.error(ex);
    return { level: "error", text: L10N.getStr("screenshotErrorCopying") };
  }
}

/**
 * Save the screenshot data to disk, returning a promise which is resolved on
 * completion.
 *
 * @param object window
 *        The DevTools Client window.
 *
 * @param object image
 *        The image object that was sent from the server.
 *
 * @return string
 *         Response message from processing the screenshot.
 */
async function saveToFile(window, image) {
  let filename = image.filename;

  // Guard against missing image data.
  if (!image.data) {
    return "";
  }

  // Check there is a .png extension to filename
  if (!filename.match(/.png$/i)) {
    filename += ".png";
  }

  const downloadsDir = await Downloads.getPreferredDownloadsDirectory();
  const downloadsDirExists = await IOUtils.exists(downloadsDir);
  if (downloadsDirExists) {
    // If filename is absolute, it will override the downloads directory and
    // still be applied as expected.
    filename = PathUtils.isAbsolute(filename)
      ? filename
      : PathUtils.joinRelative(downloadsDir, filename);
  }

  const targetFile = new FileUtils.File(filename);

  // Create download and track its progress.
  try {
    const download = await Downloads.createDownload({
      source: {
        url: image.data,
        // Here we want to know if the window in which the screenshot is taken is private.
        // We have a ChromeWindow when this is called from Browser Console (:screenshot) and
        // RDM (screenshot button).
        isPrivate: window.isChromeWindow
          ? PrivateBrowsingUtils.isWindowPrivate(window)
          : PrivateBrowsingUtils.isBrowserPrivate(
              window.browsingContext.embedderElement
            ),
      },
      target: targetFile,
    });
    const list = await Downloads.getList(Downloads.ALL);
    // add the download to the download list in the Downloads list in the Browser UI
    list.add(download);
    // Await successful completion of the save via the download manager
    await download.start();
    return { text: L10N.getFormatStr("screenshotSavedToFile", filename) };
  } catch (ex) {
    console.error(ex);
    return {
      level: "error",
      text: L10N.getFormatStr("screenshotErrorSavingToFile", filename),
    };
  }
}

module.exports = {
  captureAndSaveScreenshot,
  captureScreenshot,
  saveScreenshot,
};
