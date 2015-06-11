/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu, Ci, Cc, CC } = require("chrome");
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

XPCOMUtils.defineLazyGetter(this, "dirService", function() {
  return Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
});

XPCOMUtils.defineLazyGetter(this, "ZipWriter", function() {
  return CC("@mozilla.org/zipwriter;1", "nsIZipWriter");
});

XPCOMUtils.defineLazyGetter(this, "LocalFile", function() {
  return new CC("@mozilla.org/file/local;1", "nsILocalFile", "initWithPath");
});

XPCOMUtils.defineLazyGetter(this, "getMostRecentBrowserWindow", function() {
  return require("sdk/window/utils").getMostRecentBrowserWindow;
});

const nsIFilePicker = Ci.nsIFilePicker;

const OPEN_FLAGS = {
  RDONLY: parseInt("0x01"),
  WRONLY: parseInt("0x02"),
  CREATE_FILE: parseInt("0x08"),
  APPEND: parseInt("0x10"),
  TRUNCATE: parseInt("0x20"),
  EXCL: parseInt("0x80")
};

/**
 * Helper API for HAR export features.
 */
var HarUtils = {
  /**
   * Open File Save As dialog and let the user pick the proper file
   * location for generated HAR log.
   */
  getTargetFile: function(fileName, jsonp, compress) {
    let browser = getMostRecentBrowserWindow();

    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    fp.init(browser, null, nsIFilePicker.modeSave);
    fp.appendFilter("HTTP Archive Files", "*.har; *.harp; *.json; *.jsonp; *.zip");
    fp.appendFilters(nsIFilePicker.filterAll | nsIFilePicker.filterText);
    fp.filterIndex = 1;

    fp.defaultString = this.getHarFileName(fileName, jsonp, compress);

    let rv = fp.show();
    if (rv == nsIFilePicker.returnOK || rv == nsIFilePicker.returnReplace) {
      return fp.file;
    }

    return null;
  },

  getHarFileName: function(defaultFileName, jsonp, compress) {
    let extension = jsonp ? ".harp" : ".har";

    // Read more about toLocaleFormat & format string.
    // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/toLocaleFormat
    var now = new Date();
    var name = now.toLocaleFormat(defaultFileName);
    name = name.replace(/\:/gm, "-", "");
    name = name.replace(/\//gm, "_", "");

    let fileName = name + extension;

    // Default file extension is zip if compressing is on.
    if (compress) {
      fileName += ".zip";
    }

    return fileName;
  },

  /**
   * Save HAR string into a given file. The file might be compressed
   * if specified in the options.
   *
   * @param {File} file Target file where the HAR string (JSON)
   * should be stored.
   * @param {String} jsonString HAR data (JSON or JSONP)
   * @param {Boolean} compress The result file is zipped if set to true.
   */
  saveToFile: function(file, jsonString, compress) {
    let openFlags = OPEN_FLAGS.WRONLY | OPEN_FLAGS.CREATE_FILE |
      OPEN_FLAGS.TRUNCATE;

    try {
      let foStream = Cc["@mozilla.org/network/file-output-stream;1"]
        .createInstance(Ci.nsIFileOutputStream);

      let permFlags = parseInt("0666", 8);
      foStream.init(file, openFlags, permFlags, 0);

      let convertor = Cc["@mozilla.org/intl/converter-output-stream;1"]
        .createInstance(Ci.nsIConverterOutputStream);
      convertor.init(foStream, "UTF-8", 0, 0);

      // The entire jsonString can be huge so, write the data in chunks.
      let chunkLength = 1024 * 1024;
      for (let i=0; i<=jsonString.length; i++) {
        let data = jsonString.substr(i, chunkLength+1);
        if (data) {
          convertor.writeString(data);
        }

        i = i + chunkLength;
      }

      // this closes foStream
      convertor.close();
    } catch (err) {
      Cu.reportError(err);
      return false;
    }

    // If no compressing then bail out.
    if (!compress) {
      return true;
    }

    // Remember name of the original file, it'll be replaced by a zip file.
    let originalFilePath = file.path;
    let originalFileName = file.leafName;

    try {
      // Rename using unique name (the file is going to be removed).
      file.moveTo(null, "temp" + (new Date()).getTime() + "temphar");

      // Create compressed file with the original file path name.
      let zipFile = Cc["@mozilla.org/file/local;1"].
        createInstance(Ci.nsILocalFile);
      zipFile.initWithPath(originalFilePath);

      // The file within the zipped file doesn't use .zip extension.
      let fileName = originalFileName;
      if (fileName.indexOf(".zip") == fileName.length - 4) {
        fileName = fileName.substr(0, fileName.indexOf(".zip"));
      }

      let zip = new ZipWriter();
      zip.open(zipFile, openFlags);
      zip.addEntryFile(fileName, Ci.nsIZipWriter.COMPRESSION_DEFAULT,
        file, false);
      zip.close();

      // Remove the original file (now zipped).
      file.remove(true);
      return true;
    } catch (err) {
      Cu.reportError(err);

      // Something went wrong (disk space?) rename the original file back.
      file.moveTo(null, originalFileName);
    }

    return false;
  },

  getLocalDirectory: function(path) {
    let dir;

    if (!path) {
      dir = dirService.get("ProfD", Ci.nsILocalFile);
      dir.append("har");
      dir.append("logs");
    } else {
      dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
      dir.initWithPath(path);
    }

    return dir;
  },
}

// Exports from this module
exports.HarUtils = HarUtils;
