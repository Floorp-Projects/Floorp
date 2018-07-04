/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cr, Cu } = require("chrome");
const ChromeUtils = require("ChromeUtils");
const l10n = require("gcli/l10n");
const Services = require("Services");
const { NetUtil } = require("resource://gre/modules/NetUtil.jsm");
const { getRect } = require("devtools/shared/layout/utils");
const defer = require("devtools/shared/defer");
const { Task } = require("devtools/shared/task");

loader.lazyRequireGetter(this, "openContentLink", "devtools/client/shared/link", true);

loader.lazyImporter(this, "Downloads", "resource://gre/modules/Downloads.jsm");
loader.lazyImporter(this, "OS", "resource://gre/modules/osfile.jsm");
loader.lazyImporter(this, "FileUtils", "resource://gre/modules/FileUtils.jsm");
loader.lazyImporter(this, "PrivateBrowsingUtils",
                          "resource://gre/modules/PrivateBrowsingUtils.jsm");

// String used as an indication to generate default file name in the following
// format: "Screen Shot yyyy-mm-dd at HH.MM.SS.png"
const FILENAME_DEFAULT_VALUE = " ";
const CONTAINER_FLASHING_DURATION = 500;

/*
 * There are 2 commands and 1 converter here. The 2 commands are nearly
 * identical except that one runs on the client and one in the server.
 *
 * The server command is hidden, and is designed to be called from the client
 * command.
 */

/**
 * Both commands have the same initial filename parameter
 */
const filenameParam = {
  name: "filename",
  type: "string",
  defaultValue: FILENAME_DEFAULT_VALUE,
  description: l10n.lookup("screenshotFilenameDesc"),
  manual: l10n.lookup("screenshotFilenameManual")
};

/**
 * Both commands have almost the same set of standard optional parameters, except for the
 * type of the --selector option, which can be a node only on the server.
 */
const getScreenshotCommandParams = function(isClient) {
  return {
    group: l10n.lookup("screenshotGroupOptions"),
    params: [
      {
        name: "clipboard",
        type: "boolean",
        description: l10n.lookup("screenshotClipboardDesc"),
        manual: l10n.lookup("screenshotClipboardManual")
      },
      {
        name: "imgur",
        type: "boolean",
        description: l10n.lookup("screenshotImgurDesc"),
        manual: l10n.lookup("screenshotImgurManual")
      },
      {
        name: "delay",
        type: { name: "number", min: 0 },
        defaultValue: 0,
        description: l10n.lookup("screenshotDelayDesc"),
        manual: l10n.lookup("screenshotDelayManual")
      },
      {
        name: "dpr",
        type: { name: "number", min: 0, allowFloat: true },
        defaultValue: 0,
        description: l10n.lookup("screenshotDPRDesc"),
        manual: l10n.lookup("screenshotDPRManual")
      },
      {
        name: "fullpage",
        type: "boolean",
        description: l10n.lookup("screenshotFullPageDesc"),
        manual: l10n.lookup("screenshotFullPageManual")
      },
      {
        name: "selector",
        // On the client side, don't try to parse the selector as a node as it will
        // trigger an unsafe CPOW.
        type: isClient ? "string" : "node",
        defaultValue: null,
        description: l10n.lookup("inspectNodeDesc"),
        manual: l10n.lookup("inspectNodeManual")
      },
      {
        name: "file",
        type: "boolean",
        description: l10n.lookup("screenshotFileDesc"),
        manual: l10n.lookup("screenshotFileManual"),
      },
    ]
  };
};

const clientScreenshotParams = getScreenshotCommandParams(true);
const serverScreenshotParams = getScreenshotCommandParams(false);

exports.items = [
  {
    /**
     * Format an 'imageSummary' (as output by the screenshot command).
     * An 'imageSummary' is a simple JSON object that looks like this:
     *
     * {
     *   destinations: [ "..." ], // Required array of descriptions of the
     *                            // locations of the result image (the command
     *                            // can have multiple outputs)
     *   data: "...",             // Optional Base64 encoded image data
     *   width:1024, height:768,  // Dimensions of the image data, required
     *                            // if data != null
     *   filename: "...",         // If set, clicking the image will open the
     *                            // folder containing the given file
     *   href: "...",             // If set, clicking the image will open the
     *                            // link in a new tab
     * }
     */
    item: "converter",
    from: "imageSummary",
    to: "dom",
    exec: function(imageSummary, context) {
      const document = context.document;
      const root = document.createElement("div");

      // Add a line to the result for each destination
      imageSummary.destinations.forEach(destination => {
        const title = document.createElement("div");
        title.textContent = destination;
        root.appendChild(title);
      });

      // Add the thumbnail image
      if (imageSummary.data != null) {
        const image = context.document.createElement("div");
        const previewHeight = parseInt(256 * imageSummary.height / imageSummary.width,
                                       10);
        const style = "" +
            "width: 256px;" +
            "height: " + previewHeight + "px;" +
            "max-height: 256px;" +
            "background-image: url('" + imageSummary.data + "');" +
            "background-size: 256px " + previewHeight + "px;" +
            "margin: 4px;" +
            "display: block;";
        image.setAttribute("style", style);
        root.appendChild(image);
      }

      // Click handler
      if (imageSummary.href || imageSummary.filename) {
        root.style.cursor = "pointer";
        root.addEventListener("click", () => {
          if (imageSummary.href) {
            openContentLink(imageSummary.href);
          } else if (imageSummary.filename) {
            const file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
            file.initWithPath(imageSummary.filename);
            file.reveal();
          }
        });
      }

      return root;
    }
  },
  {
    item: "command",
    runAt: "client",
    name: "screenshot",
    description: l10n.lookup("screenshotDesc"),
    manual: l10n.lookup("screenshotManual"),
    returnType: "imageSummary",
    buttonId: "command-button-screenshot",
    buttonClass: "command-button",
    tooltipText: l10n.lookup("screenshotTooltipPage"),
    params: [
      filenameParam,
      clientScreenshotParams,
    ],
    exec: function(args, context) {
      // Re-execute the command on the server
      const command = context.typed.replace(/^screenshot/, "screenshot_server");
      const capture = context.updateExec(command).then(output => {
        return output.error ? Promise.reject(output.data) : output.data;
      });

      simulateCameraEffect(context.environment.chromeDocument, "shutter");
      return capture.then(saveScreenshot.bind(null, args, context));
    },
  },
  {
    item: "command",
    runAt: "server",
    name: "screenshot_server",
    hidden: true,
    returnType: "imageSummary",
    params: [
      filenameParam,
      serverScreenshotParams,
    ],
    exec: function(args, context) {
      return captureScreenshot(args, context.environment.document);
    },
  }
];

/**
 * This function is called to simulate camera effects
 */
function simulateCameraEffect(document, effect) {
  const window = document.defaultView;
  if (effect === "shutter") {
    if (Services.prefs.getBoolPref("devtools.screenshot.audio.enabled")) {
      const audioCamera = new window.Audio("resource://devtools/client/themes/audio/shutter.wav");
      audioCamera.play();
    }
  }
  if (effect == "flash") {
    const frames = Cu.cloneInto({ opacity: [ 0, 1 ] }, window);
    document.documentElement.animate(frames, CONTAINER_FLASHING_DURATION);
  }
}

/**
 * This function simply handles the --delay argument before calling
 * createScreenshotData
 */
function captureScreenshot(args, document) {
  if (args.delay > 0) {
    return new Promise((resolve, reject) => {
      document.defaultView.setTimeout(() => {
        createScreenshotData(document, args).then(resolve, reject);
      }, args.delay * 1000);
    });
  }
  return createScreenshotData(document, args);
}

/**
 * There are several possible destinations for the screenshot, SKIP is used
 * in saveScreenshot() whenever one of them is not used
 */
const SKIP = Promise.resolve();

/**
 * Save the captured screenshot to one of several destinations.
 */
function saveScreenshot(args, context, reply) {
  const fileNeeded = args.filename != FILENAME_DEFAULT_VALUE ||
    (!args.imgur && !args.clipboard) || args.file;

  return Promise.all([
    args.clipboard ? saveToClipboard(context, reply) : SKIP,
    args.imgur ? uploadToImgur(reply) : SKIP,
    fileNeeded ? saveToFile(context, reply) : SKIP,
  ]).then(() => reply);
}

/**
 * This does the dirty work of creating a base64 string out of an
 * area of the browser window
 */
function createScreenshotData(document, args) {
  const window = document.defaultView;
  let left = 0;
  let top = 0;
  let width;
  let height;
  const currentX = window.scrollX;
  const currentY = window.scrollY;

  let filename = getFilename(args.filename);

  if (args.fullpage) {
    // Bug 961832: GCLI screenshot shows fixed position element in wrong
    // position if we don't scroll to top
    window.scrollTo(0, 0);
    width = window.innerWidth + window.scrollMaxX - window.scrollMinX;
    height = window.innerHeight + window.scrollMaxY - window.scrollMinY;
    filename = filename.replace(".png", "-fullpage.png");
  } else if (args.selector) {
    ({ top, left, width, height } = getRect(window, args.selector, window));
  } else {
    left = window.scrollX;
    top = window.scrollY;
    width = window.innerWidth;
    height = window.innerHeight;
  }

  // Only adjust for scrollbars when considering the full window
  if (!args.selector) {
    const winUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils);
    const scrollbarHeight = {};
    const scrollbarWidth = {};
    winUtils.getScrollbarSize(false, scrollbarWidth, scrollbarHeight);
    width -= scrollbarWidth.value;
    height -= scrollbarHeight.value;
  }

  const canvas = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
  const ctx = canvas.getContext("2d");
  const ratio = args.dpr ? args.dpr : window.devicePixelRatio;
  canvas.width = width * ratio;
  canvas.height = height * ratio;
  ctx.scale(ratio, ratio);
  ctx.drawWindow(window, left, top, width, height, "#fff");
  const data = canvas.toDataURL("image/png", "");

  // See comment above on bug 961832
  if (args.fullpage) {
    window.scrollTo(currentX, currentY);
  }

  simulateCameraEffect(document, "flash");

  return Promise.resolve({
    destinations: [],
    data: data,
    height: height,
    width: width,
    filename: filename,
  });
}

/**
 * We may have a filename specified in args, or we might have to generate
 * one.
 */
function getFilename(defaultName) {
  // Create a name for the file if not present
  if (defaultName != FILENAME_DEFAULT_VALUE) {
    return defaultName;
  }

  const date = new Date();
  let dateString = date.getFullYear() + "-" + (date.getMonth() + 1) +
                  "-" + date.getDate();
  dateString = dateString.split("-").map(function(part) {
    if (part.length == 1) {
      part = "0" + part;
    }
    return part;
  }).join("-");

  const timeString = date.toTimeString().replace(/:/g, ".").split(" ")[0];
  return l10n.lookupFormat("screenshotGeneratedFilename",
                           [ dateString, timeString ]) + ".png";
}

/**
 * Save the image data to the clipboard. This returns a promise, so it can
 * be treated exactly like imgur / file processing, but it's really sync
 * for now.
 */
function saveToClipboard(context, reply) {
  return new Promise(resolve => {
    try {
      const channel = NetUtil.newChannel({
        uri: reply.data,
        loadUsingSystemPrincipal: true,
        contentPolicyType: Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE
      });
      const input = channel.open2();

      const loadContext = context.environment.chromeWindow
                                 .QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIWebNavigation)
                                 .QueryInterface(Ci.nsILoadContext);

      const callback = {
        onImageReady(container, status) {
          if (!container) {
            console.error("imgTools.decodeImageAsync failed");
            reply.destinations.push(l10n.lookup("screenshotErrorCopying"));
            resolve();
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

            reply.destinations.push(l10n.lookup("screenshotCopied"));
          } catch (ex) {
            console.error(ex);
            reply.destinations.push(l10n.lookup("screenshotErrorCopying"));
          }
          resolve();
        }
      };

      const threadManager = Cc["@mozilla.org/thread-manager;1"].getService();
      const imgTools = Cc["@mozilla.org/image/tools;1"]
                          .getService(Ci.imgITools);
      imgTools.decodeImageAsync(input, channel.contentType, callback,
                                threadManager.currentThread);
    } catch (ex) {
      console.error(ex);
      reply.destinations.push(l10n.lookup("screenshotErrorCopying"));
      resolve();
    }
  });
}

/**
 * Upload screenshot data to Imgur, returning a promise of a URL (as a string)
 */
function uploadToImgur(reply) {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    const fd = new FormData();
    fd.append("image", reply.data.split(",")[1]);
    fd.append("type", "base64");
    fd.append("title", reply.filename);

    const postURL = Services.prefs.getCharPref("devtools.gcli.imgurUploadURL");
    const clientID = "Client-ID " +
                     Services.prefs.getCharPref("devtools.gcli.imgurClientID");

    xhr.open("POST", postURL);
    xhr.setRequestHeader("Authorization", clientID);
    xhr.send(fd);
    xhr.responseType = "json";

    xhr.onreadystatechange = function() {
      if (xhr.readyState == 4) {
        if (xhr.status == 200) {
          reply.href = xhr.response.data.link;
          reply.destinations.push(l10n.lookupFormat("screenshotImgurUploaded",
                                                    [ reply.href ]));
        } else {
          reply.destinations.push(l10n.lookup("screenshotImgurError"));
        }

        resolve();
      }
    };
  });
}

/**
 * Progress listener that forwards calls to a transfer object.
 *
 * This is used below in saveToFile to forward progress updates from the
 * nsIWebBrowserPersist object that does the actual saving to the nsITransfer
 * which just represents the operation for the Download Manager.  This keeps the
 * Download Manager updated on saving progress and completion, so that it gives
 * visual feedback from the downloads toolbar button when the save is done.
 *
 * It also allows the browser window to show auth prompts if needed (should not
 * be needed for saving screenshots).
 *
 * This code is borrowed directly from contentAreaUtils.js.
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
  this._completedDeferred = defer();
  this.completed = this._completedDeferred.promise;
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
 */
var saveToFile = Task.async(function* (context, reply) {
  const document = context.environment.chromeDocument;
  const window = context.environment.chromeWindow;

  // Check there is a .png extension to filename
  if (!reply.filename.match(/.png$/i)) {
    reply.filename += ".png";
  }

  const downloadsDir = yield Downloads.getPreferredDownloadsDirectory();
  const downloadsDirExists = yield OS.File.exists(downloadsDir);
  if (downloadsDirExists) {
    // If filename is absolute, it will override the downloads directory and
    // still be applied as expected.
    reply.filename = OS.Path.join(downloadsDir, reply.filename);
  }

  const sourceURI = Services.io.newURI(reply.data);
  const targetFile = new FileUtils.File(reply.filename);
  const targetFileURI = Services.io.newFileURI(targetFile);

  // Create download and track its progress.
  // This is adapted from saveURL in contentAreaUtils.js, but simplified greatly
  // and modified to allow saving to arbitrary paths on disk.  Using these
  // objects as opposed to just writing with OS.File allows us to tie into the
  // download manager to record a download entry and to get visual feedback from
  // the downloads toolbar button when the save is done.
  const nsIWBP = Ci.nsIWebBrowserPersist;
  const flags = nsIWBP.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
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
  const principal = Services.scriptSecurityManager.getSystemPrincipal();
  persist.savePrivacyAwareURI(sourceURI,
                              principal,
                              0,
                              document.documentURIObject,
                              Ci.nsIHttpChannel.REFERRER_POLICY_UNSET,
                              null,
                              null,
                              targetFileURI,
                              isPrivate);

  try {
    // Await successful completion of the save via the listener
    yield listener.completed;
    reply.destinations.push(l10n.lookup("screenshotSavedToFile") +
                            ` "${reply.filename}"`);
  } catch (ex) {
    console.error(ex);
    reply.destinations.push(l10n.lookup("screenshotErrorSavingToFile") + " " +
                            reply.filename);
  }
});
