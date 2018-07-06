/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci } = require("chrome");
const { LocalizationHelper } = require("devtools/shared/l10n");
const Services = require("Services");

loader.lazyImporter(this, "Downloads", "resource://gre/modules/Downloads.jsm");
loader.lazyImporter(this, "OS", "resource://gre/modules/osfile.jsm");
loader.lazyImporter(this, "FileUtils", "resource://gre/modules/FileUtils.jsm");

const STRINGS_URI = "devtools/shared/locales/screenshot.properties";
const L10N = new LocalizationHelper(STRINGS_URI);

const screenshotDescription = L10N.getStr("screenshotDesc");
const screenshotGroupOptions = L10N.getStr("screenshotGroupOptions");
const screenshotCommandParams = [
  {
    name: "clipboard",
    type: "boolean",
    description: L10N.getStr("screenshotClipboardDesc"),
    manual: L10N.getStr("screenshotClipboardManual")
  },
  {
    name: "delay",
    type: "number",
    description: L10N.getStr("screenshotDelayDesc"),
    manual: L10N.getStr("screenshotDelayManual")
  },
  {
    name: "dpr",
    type: "number",
    description: L10N.getStr("screenshotDPRDesc"),
    manual: L10N.getStr("screenshotDPRManual")
  },
  {
    name: "fullpage",
    type: "boolean",
    description: L10N.getStr("screenshotFullPageDesc"),
    manual: L10N.getStr("screenshotFullPageManual")
  },
  {
    name: "selector",
    type: "string",
    description: L10N.getStr("inspectNodeDesc"),
    manual: L10N.getStr("inspectNodeManual")
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
    manual: L10N.getStr("screenshotFilenameManual")
  }
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
  return Object.entries(param).map(([key, value]) => {
    if (key === "name") {
      const name = `${padding}--${value}`;
      return name;
    }
    return `${padding.repeat(2)}${key}: ${value}`;
  }).join("\n");
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
 * called with and the image value from the server, and uses the client window to save
 * the screenshot to the remote debugging machine's memory or clipboard.
 *
 * @param object window
 *        The Debugger Client window.
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
function processScreenshot(window, args = {}, value) {
  if (args.help) {
    const message = getFormattedHelpData();
    // Wrap meesage in an array so that the return value is consistant with saveScreenshot
    return [message];
  }
  simulateCameraShutter(window.document);
  return saveScreenshot(window, args, value);
}

/**
 * This function is called to simulate camera effects
 *
 * @param object document
 *        The Debugger Client document.
 */
function simulateCameraShutter(document) {
  const window = document.defaultView;
  if (Services.prefs.getBoolPref("devtools.screenshot.audio.enabled")) {
    const audioCamera = new window.Audio("resource://devtools/client/themes/audio/shutter.wav");
    audioCamera.play();
  }
}

/**
 * Save the captured screenshot to one of several destinations.
 *
 * @param object window
 *        The Debugger Client window.
 *
 * @param object args
 *        The original args with which the screenshot was called.
 *
 * @param object image
 *        The image object that was sent from the server.
 *
 * @return string[]
 *         Response messages from processing the screenshot.
 */
async function saveScreenshot(window, args, image) {
  const fileNeeded = args.filename ||
    !args.clipboard || args.file;
  const results = [];

  if (args.clipboard) {
    const result = saveToClipboard(window, image.data);
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
 * @param object window
 *        The Debugger Client window.
 *
 * @param string base64URI
 *        The image data encoded in a base64 URI that was sent from the server.
 *
 * @return string
 *         Response message from processing the screenshot.
 */
function saveToClipboard(window, base64URI) {
  try {
    const imageTools = Cc["@mozilla.org/image/tools;1"]
                       .getService(Ci.imgITools);

    const base64Data = base64URI.replace("data:image/png;base64,", "");

    const image = atob(base64Data);
    const imgPtr = Cc["@mozilla.org/supports-interface-pointer;1"]
                   .createInstance(Ci.nsISupportsInterfacePointer);
    imgPtr.data = imageTools.decodeImageFromBuffer(image, image.length, "image/png");

    const transferable = Cc["@mozilla.org/widget/transferable;1"]
                     .createInstance(Ci.nsITransferable);
    transferable.init(null);
    transferable.addDataFlavor("image/png");
    transferable.setTransferData("image/png", imgPtr, -1);

    Services.clipboard.setData(transferable, null, Services.clipboard.kGlobalClipboard);
    return L10N.getStr("screenshotCopied");
  } catch (ex) {
    console.error(ex);
    return L10N.getStr("screenshotErrorCopying");
  }
}

/**
 * Save the screenshot data to disk, returning a promise which is resolved on
 * completion.
 *
 * @param object window
 *        The Debugger Client window.
 *
 * @param object image
 *        The image object that was sent from the server.
 *
 * @return string
 *         Response message from processing the screenshot.
 */
async function saveToFile(window, image) {
  let filename = image.filename;

  // Check there is a .png extension to filename
  if (!filename.match(/.png$/i)) {
    filename += ".png";
  }

  const downloadsDir = await Downloads.getPreferredDownloadsDirectory();
  const downloadsDirExists = await OS.File.exists(downloadsDir);
  if (downloadsDirExists) {
    // If filename is absolute, it will override the downloads directory and
    // still be applied as expected.
    filename = OS.Path.join(downloadsDir, filename);
  }

  const sourceURI = Services.io.newURI(image.data);
  const targetFile = new FileUtils.File(filename);

  // Create download and track its progress.
  try {
    const download = await Downloads.createDownload({
      source: sourceURI,
      target: targetFile
    });
    const list = await Downloads.getList(Downloads.ALL);
    // add the download to the download list in the Downloads list in the Browser UI
    list.add(download);
    // Await successful completion of the save via the download manager
    await download.start();
    return L10N.getFormatStr("screenshotSavedToFile", filename);
  } catch (ex) {
    console.error(ex);
    return L10N.getFormatStr("screenshotErrorSavingToFile", filename);
  }
}

module.exports = processScreenshot;
