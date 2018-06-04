/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cr } = require("chrome");
const ChromeUtils = require("ChromeUtils");
const { LocalizationHelper } = require("devtools/shared/l10n");
const Services = require("Services");
const { NetUtil } = require("resource://gre/modules/NetUtil.jsm");

loader.lazyImporter(this, "Downloads", "resource://gre/modules/Downloads.jsm");
loader.lazyImporter(this, "OS", "resource://gre/modules/osfile.jsm");
loader.lazyImporter(this, "FileUtils", "resource://gre/modules/FileUtils.jsm");
loader.lazyImporter(this, "PrivateBrowsingUtils",
                          "resource://gre/modules/PrivateBrowsingUtils.jsm");

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
    const result = await saveToClipboard(window, image.data);
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
 * @param string data
 *        The image data encoded in base64 that was sent from the server.
 *
 * @return string
 *         Response message from processing the screenshot.
 */
function saveToClipboard(window, data) {
  return new Promise(resolve => {
    try {
      const channel = NetUtil.newChannel({
        uri: data,
        loadUsingSystemPrincipal: true,
        contentPolicyType: Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE
      });
      const input = channel.open2();

      const loadContext = window.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIWebNavigation)
                                .QueryInterface(Ci.nsILoadContext);

      const callback = {
        onImageReady(container, status) {
          if (!container) {
            console.error("imgTools.decodeImageAsync failed");
            resolve(L10N.getStr("screenshotErrorCopying"));
            return;
          }

          try {
            const wrapped = Cc["@mozilla.org/supports-interface-pointer;1"]
                              .createInstance(Ci.nsISupportsInterfacePointer);
            wrapped.data = container;

            const trans = Cc["@mozilla.org/widget/transferable;1"]
                            .createInstance(Ci.nsITransferable);
            trans.init(loadContext);
            trans.addDataFlavor(channel.contentType);
            trans.setTransferData(channel.contentType, wrapped, -1);

            Services.clipboard.setData(trans, null, Ci.nsIClipboard.kGlobalClipboard);

            resolve(L10N.getStr("screenshotCopied"));
          } catch (ex) {
            console.error(ex);
            resolve(L10N.getStr("screenshotErrorCopying"));
          }
        }
      };

      const threadManager = Cc["@mozilla.org/thread-manager;1"].getService();
      const imgTools = Cc["@mozilla.org/image/tools;1"]
                          .getService(Ci.imgITools);
      imgTools.decodeImageAsync(input, channel.contentType, callback,
                                threadManager.currentThread);
    } catch (ex) {
      console.error(ex);
      resolve(L10N.getStr("screenshotErrorCopying"));
    }
  });
}

/**
 * Progress listener that forwards calls to a transfer object.
 *
 * This is used below in saveToFile to forward progress updates from the
 * nsIWebBrowserPersist object that does the actual saving to the nsITransfer
 * which just represents the operation for the Download Manager. This keeps the
 * Download Manager updated on saving progress and completion, so that it gives
 * visual feedback from the downloads toolbar button when the save is done.
 *
 * It also allows the browser window to show auth prompts if needed (should not
 * be needed for saving screenshots).
 *
 * This code is borrowed directly from contentAreaUtils.js.
 *
 * @param object win
 *        The Debugger Client window.
 *
 * @param object transfer
 *        The transfer object.
 *
 */
function DownloadListener(win, transfer) {
  this.window = win;
  this.transfer = transfer;

  // For most method calls, forward to the transfer object.
  for (const name in transfer) {
    if (name != "QueryInterface" &&
        name != "onStateChange") {
      this[name] = (...args) => transfer[name].apply(transfer, args);
    }
  }

  // Allow saveToFile to await completion for error handling
  this._completedDeferred = {};
  this.completed = new Promise((resolve, reject) => {
    this._completedDeferred.resolve = resolve;
    this._completedDeferred.reject = reject;
  });
}

DownloadListener.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIInterfaceRequestor",
                                          "nsIWebProgressListener",
                                          "nsIWebProgressListener2"]),

  getInterface: function(iid) {
    if (iid.equals(Ci.nsIAuthPrompt) ||
        iid.equals(Ci.nsIAuthPrompt2)) {
      const ww = Cc["@mozilla.org/embedcomp/window-watcher;1"]
                 .getService(Ci.nsIPromptFactory);
      return ww.getPrompt(this.window, iid);
    }

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  onStateChange: function(webProgress, request, state, status) {
    // Check if the download has completed
    if ((state & Ci.nsIWebProgressListener.STATE_STOP) &&
        (state & Ci.nsIWebProgressListener.STATE_IS_NETWORK)) {
      if (status == Cr.NS_OK) {
        this._completedDeferred.resolve();
      } else {
        this._completedDeferred.reject();
      }
    }

    this.transfer.onStateChange.apply(this.transfer, arguments);
  }
};

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
  const document = window.document;
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
  const targetFileURI = Services.io.newFileURI(targetFile);

  // Create download and track its progress.
  // This is adapted from saveURL in contentAreaUtils.js, but simplified greatly
  // and modified to allow saving to arbitrary paths on disk. Using these
  // objects as opposed to just writing with OS.File allows us to tie into the
  // download manager to record a download entry and to get visual feedback from
  // the downloads toolbar button when the save is done.
  const nsIWBP = Ci.nsIWebBrowserPersist;
  const flags = nsIWBP.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
                nsIWBP.PERSIST_FLAGS_FORCE_ALLOW_COOKIES |
                nsIWBP.PERSIST_FLAGS_BYPASS_CACHE |
                nsIWBP.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;
  const isPrivate =
    PrivateBrowsingUtils.isContentWindowPrivate(document.defaultView);
  const persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
                  .createInstance(Ci.nsIWebBrowserPersist);
  persist.persistFlags = flags;
  const tr = Cc["@mozilla.org/transfer;1"].createInstance(Ci.nsITransfer);
  tr.init(sourceURI,
          targetFileURI,
          "",
          null,
          null,
          null,
          persist,
          isPrivate);
  const listener = new DownloadListener(window, tr);
  persist.progressListener = listener;
  persist.savePrivacyAwareURI(sourceURI,
                              0,
                              document.documentURIObject,
                              Ci.nsIHttpChannel.REFERRER_POLICY_UNSET,
                              null,
                              null,
                              targetFileURI,
                              isPrivate);

  try {
    // Await successful completion of the save via the listener
    await listener.completed;
    return L10N.getFormatStr("screenshotSavedToFile", filename);
  } catch (ex) {
    console.error(ex);
    return L10N.getFormatStr("screenshotErrorSavingToFile", filename);
  }
}

module.exports = processScreenshot;
