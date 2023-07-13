/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  DownloadLastDir: "resource://gre/modules/DownloadLastDir.sys.mjs",
  DownloadPaths: "resource://gre/modules/DownloadPaths.sys.mjs",
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
  ScreenshotsUtils: "resource:///modules/ScreenshotsUtils.sys.mjs",
});

/**
 * Gets the filename automatically or by a file picker depending on "browser.download.useDownloadDir"
 * @param filenameTitle The title of the current page
 * @param browser The current browser
 * @returns Path of the chosen filename
 */
export async function getFilename(filenameTitle, browser) {
  if (filenameTitle === null) {
    filenameTitle = await lazy.ScreenshotsUtils.getActor(browser).sendQuery(
      "Screenshots:getDocumentTitle"
    );
  }
  const date = new Date();
  /* eslint-disable no-control-regex */
  filenameTitle = filenameTitle
    .replace(/[\\/]/g, "_")
    .replace(/[\u200e\u200f\u202a-\u202e]/g, "")
    .replace(/[\x00-\x1f\x7f-\x9f:*?|"<>;,+=\[\]]+/g, " ")
    .replace(/^[\s\u180e.]+|[\s\u180e.]+$/g, "");
  /* eslint-enable no-control-regex */
  filenameTitle = filenameTitle.replace(/\s{1,4000}/g, " ");
  const currentDateTime = new Date(
    date.getTime() - date.getTimezoneOffset() * 60 * 1000
  ).toISOString();
  const filenameDate = currentDateTime.substring(0, 10);
  const filenameTime = currentDateTime.substring(11, 19).replace(/:/g, "-");
  let clipFilename = `Screenshot ${filenameDate} at ${filenameTime} ${filenameTitle}`;

  // Crop the filename size at less than 246 bytes, so as to leave
  // room for the extension and an ellipsis [...]. Note that JS
  // strings are UTF16 but the filename will be converted to UTF8
  // when saving which could take up more space, and we want a
  // maximum of 255 bytes (not characters). Here, we iterate
  // and crop at shorter and shorter points until we fit into
  // 255 bytes.
  let suffix = "";
  for (let cropSize = 246; cropSize >= 0; cropSize -= 32) {
    if (new Blob([clipFilename]).size > 246) {
      clipFilename = clipFilename.substring(0, cropSize);
      suffix = "[...]";
    } else {
      break;
    }
  }

  clipFilename += suffix;

  let extension = ".png";
  let filename = clipFilename + extension;

  let useDownloadDir = Services.prefs.getBoolPref(
    "browser.download.useDownloadDir"
  );
  if (useDownloadDir) {
    const downloadsDir = await lazy.Downloads.getPreferredDownloadsDirectory();
    const downloadsDirExists = await IOUtils.exists(downloadsDir);
    if (downloadsDirExists) {
      // If filename is absolute, it will override the downloads directory and
      // still be applied as expected.
      filename = PathUtils.join(downloadsDir, filename);
    }
  } else {
    let fileInfo = new FileInfo(filename);
    let file;

    let fpParams = {
      fpTitleKey: "SaveImageTitle",
      fileInfo,
      contentType: "image/png",
      saveAsType: 0,
      file,
    };

    let accepted = await promiseTargetFile(fpParams, browser.ownerGlobal);
    if (!accepted) {
      return null;
    }

    filename = fpParams.file.path;
  }

  return filename;
}

// The below functions are a modified copy from toolkit/content/contentAreaUtils.js
/**
 * Structure for holding info about a URL and the target filename it should be
 * saved to.
 * @param aFileName The target filename
 */
class FileInfo {
  constructor(aFileName) {
    this.fileName = aFileName;
    this.fileBaseName = aFileName.replace(".png", "");
    this.fileExt = "png";
  }
}

const ContentAreaUtils = {
  get stringBundle() {
    delete this.stringBundle;
    return (this.stringBundle = Services.strings.createBundle(
      "chrome://global/locale/contentAreaCommands.properties"
    ));
  },
};

function makeFilePicker() {
  const fpContractID = "@mozilla.org/filepicker;1";
  const fpIID = Ci.nsIFilePicker;
  return Cc[fpContractID].createInstance(fpIID);
}

function getMIMEService() {
  const mimeSvcContractID = "@mozilla.org/mime;1";
  const mimeSvcIID = Ci.nsIMIMEService;
  const mimeSvc = Cc[mimeSvcContractID].getService(mimeSvcIID);
  return mimeSvc;
}

function getMIMEInfoForType(aMIMEType, aExtension) {
  if (aMIMEType || aExtension) {
    try {
      return getMIMEService().getFromTypeAndExtension(aMIMEType, aExtension);
    } catch (e) {}
  }
  return null;
}

// This is only used after the user has entered a filename.
function validateFileName(aFileName) {
  let processed =
    lazy.DownloadPaths.sanitize(aFileName, {
      compressWhitespaces: false,
      allowInvalidFilenames: true,
    }) || "_";
  if (AppConstants.platform == "android") {
    // If a large part of the filename has been sanitized, then we
    // will use a default filename instead
    if (processed.replace(/_/g, "").length <= processed.length / 2) {
      // We purposefully do not use a localized default filename,
      // which we could have done using
      // ContentAreaUtils.stringBundle.GetStringFromName("UntitledSaveFileName")
      // since it may contain invalid characters.
      let original = processed;
      processed = "download";

      // Preserve a suffix, if there is one
      if (original.includes(".")) {
        let suffix = original.split(".").pop();
        if (suffix && !suffix.includes("_")) {
          processed += "." + suffix;
        }
      }
    }
  }
  return processed;
}

function appendFiltersForContentType(
  aFilePicker,
  aContentType,
  aFileExtension
) {
  let mimeInfo = getMIMEInfoForType(aContentType, aFileExtension);
  if (mimeInfo) {
    let extString = "";
    for (let extension of mimeInfo.getFileExtensions()) {
      if (extString) {
        extString += "; ";
      } // If adding more than one extension,
      // separate by semi-colon
      extString += "*." + extension;
    }

    if (extString) {
      aFilePicker.appendFilter(mimeInfo.description, extString);
    }
  }

  // Always append the all files (*) filter
  aFilePicker.appendFilters(Ci.nsIFilePicker.filterAll);
}

/**
 * Given the Filepicker Parameters (aFpP), show the file picker dialog,
 * prompting the user to confirm (or change) the fileName.
 * @param aFpP
 *        A structure (see definition in internalSave(...) method)
 *        containing all the data used within this method.
 * @param win
 *        The window used for opening the file picker
 * @return Promise
 * @resolve a boolean. When true, it indicates that the file picker dialog
 *          is accepted.
 */
function promiseTargetFile(aFpP, win) {
  return (async function () {
    let downloadLastDir = new lazy.DownloadLastDir(win);

    // Default to the user's default downloads directory configured
    // through download prefs.
    let dirPath = await lazy.Downloads.getPreferredDownloadsDirectory();
    let dirExists = await IOUtils.exists(dirPath);
    let dir = new lazy.FileUtils.File(dirPath);

    // We must prompt for the file name explicitly.
    // If we must prompt because we were asked to...
    let file = await downloadLastDir.getFileAsync(null);
    if (file && (await IOUtils.exists(file.path))) {
      dir = file;
      dirExists = true;
    }

    if (!dirExists) {
      // Default to desktop.
      dir = Services.dirsvc.get("Desk", Ci.nsIFile);
    }

    let fp = makeFilePicker();
    let titleKey = aFpP.fpTitleKey;
    fp.init(
      win,
      ContentAreaUtils.stringBundle.GetStringFromName(titleKey),
      Ci.nsIFilePicker.modeSave
    );

    fp.displayDirectory = dir;
    fp.defaultExtension = aFpP.fileInfo.fileExt;
    fp.defaultString = aFpP.fileInfo.fileName;
    appendFiltersForContentType(fp, aFpP.contentType, aFpP.fileInfo.fileExt);

    let result = await new Promise(resolve => {
      fp.open(function (aResult) {
        resolve(aResult);
      });
    });
    if (result == Ci.nsIFilePicker.returnCancel || !fp.file) {
      return false;
    }

    // Do not store the last save directory as a pref inside the private browsing mode
    downloadLastDir.setFile(null, fp.file.parent);

    aFpP.saveAsType = fp.filterIndex;
    aFpP.file = fp.file;
    aFpP.file.leafName = validateFileName(aFpP.file.leafName);

    return true;
  })();
}
