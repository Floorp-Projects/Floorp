/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cu, Cc, Ci} = require("chrome");
const {getMostRecentBrowserWindow} = require("sdk/window/utils");

const OPEN_FLAGS = {
  RDONLY: parseInt("0x01"),
  WRONLY: parseInt("0x02"),
  CREATE_FILE: parseInt("0x08"),
  APPEND: parseInt("0x10"),
  TRUNCATE: parseInt("0x20"),
  EXCL: parseInt("0x80")
};

/**
 * Open File Save As dialog and let the user to pick proper file location.
 */
exports.getTargetFile = function() {
  var fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);

  var win = getMostRecentBrowserWindow();
  fp.init(win, null, Ci.nsIFilePicker.modeSave);
  fp.appendFilter("JSON Files","*.json; *.jsonp;");
  fp.appendFilters(Ci.nsIFilePicker.filterText);
  fp.appendFilters(Ci.nsIFilePicker.filterAll);
  fp.filterIndex = 0;

  var rv = fp.show();
  if (rv == Ci.nsIFilePicker.returnOK || rv == Ci.nsIFilePicker.returnReplace) {
    return fp.file;
  }

  return null;
}

/**
 * Save JSON to a file
 */
exports.saveToFile = function(file, jsonString) {
  var foStream = Cc["@mozilla.org/network/file-output-stream;1"].
    createInstance(Ci.nsIFileOutputStream);

  // write, create, truncate
  let openFlags = OPEN_FLAGS.WRONLY | OPEN_FLAGS.CREATE_FILE |
    OPEN_FLAGS.TRUNCATE;

  let permFlags = parseInt("0666", 8);
  foStream.init(file, openFlags, permFlags, 0);

  var converter = Cc["@mozilla.org/intl/converter-output-stream;1"].
    createInstance(Ci.nsIConverterOutputStream);

  converter.init(foStream, "UTF-8", 0, 0);

  // The entire jsonString can be huge so, write the data in chunks.
  var chunkLength = 1024*1204;
  for (var i=0; i<=jsonString.length; i++) {
    var data = jsonString.substr(i, chunkLength+1);
    if (data) {
      converter.writeString(data);
    }
    i = i + chunkLength;
  }

  // this closes foStream
  converter.close();
}

/**
 * Get the current theme from preferences.
 */
exports.getCurrentTheme = function() {
  return Services.prefs.getCharPref("devtools.theme");
}

/**
 * Export given object into the target window scope.
 */
exports.exportIntoContentScope = function(win, obj, defineAs) {
  var clone = Cu.createObjectIn(win, {
    defineAs: defineAs
  });

  var props = Object.getOwnPropertyNames(obj);
  for (var i=0; i<props.length; i++) {
    var propName = props[i];
    var propValue = obj[propName];
    if (typeof propValue == "function") {
      Cu.exportFunction(propValue, clone, {
        defineAs: propName
      });
    }
  }
}
