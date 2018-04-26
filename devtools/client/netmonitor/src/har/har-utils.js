/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable mozilla/reject-some-requires */

"use strict";

const Services = require("Services");
const { Ci, Cc, CC } = require("chrome");
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "ZipWriter", function() {
  return CC("@mozilla.org/zipwriter;1", "nsIZipWriter");
});

const OPEN_FLAGS = {
  RDONLY: parseInt("0x01", 16),
  WRONLY: parseInt("0x02", 16),
  CREATE_FILE: parseInt("0x08", 16),
  APPEND: parseInt("0x10", 16),
  TRUNCATE: parseInt("0x20", 16),
  EXCL: parseInt("0x80", 16)
};

function formatDate(date) {
  let year = String(date.getFullYear() % 100).padStart(2, "0");
  let month = String(date.getMonth() + 1).padStart(2, "0");
  let day = String(date.getDate()).padStart(2, "0");
  let hour = String(date.getHours()).padStart(2, "0");
  let minutes = String(date.getMinutes()).padStart(2, "0");
  let seconds = String(date.getSeconds()).padStart(2, "0");

  return `${year}-${month}-${day} ${hour}-${minutes}-${seconds}`;
}

/**
 * Helper API for HAR export features.
 */
var HarUtils = {
  getHarFileName: function(defaultFileName, jsonp, compress) {
    let extension = jsonp ? ".harp" : ".har";

    let now = new Date();
    let name = defaultFileName.replace(/%date/g, formatDate(now));
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
      convertor.init(foStream, "UTF-8");

      // The entire jsonString can be huge so, write the data in chunks.
      let chunkLength = 1024 * 1024;
      for (let i = 0; i <= jsonString.length; i++) {
        let data = jsonString.substr(i, chunkLength + 1);
        if (data) {
          convertor.writeString(data);
        }

        i = i + chunkLength;
      }

      // this closes foStream
      convertor.close();
    } catch (err) {
      console.error(err);
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
      let zipFile = Cc["@mozilla.org/file/local;1"]
        .createInstance(Ci.nsIFile);
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
      console.error(err);

      // Something went wrong (disk space?) rename the original file back.
      file.moveTo(null, originalFileName);
    }

    return false;
  },

  getLocalDirectory: function(path) {
    let dir;

    if (!path) {
      dir = Services.dirsvc.get("ProfD", Ci.nsIFile);
      dir.append("har");
      dir.append("logs");
    } else {
      dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
      dir.initWithPath(path);
    }

    return dir;
  },
};

// Exports from this module
exports.HarUtils = HarUtils;
