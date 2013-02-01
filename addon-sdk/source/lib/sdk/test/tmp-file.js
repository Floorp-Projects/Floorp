/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci } = require("chrome");

const system = require("sdk/system");
const file = require("sdk/io/file");
const unload = require("sdk/system/unload");

// Retrieve the path to the OS temporary directory:
const tmpDir = require("sdk/system").pathFor("TmpD");

// List of all tmp file created
let files = [];

// Remove all tmp files on addon disabling
unload.when(function () {
  files.forEach(function (path){
    // Catch exception in order to avoid leaking following files
    try {
      if (file.exists(path))
        file.remove(path);
    }
    catch(e) {
      console.exception(e);
    }
  });
});

// Utility function that synchronously reads local resource from the given
// `uri` and returns content string. Read in binary mode.
function readBinaryURI(uri) {
  let ioservice = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  let channel = ioservice.newChannel(uri, "UTF-8", null);
  let stream = Cc["@mozilla.org/binaryinputstream;1"].
               createInstance(Ci.nsIBinaryInputStream);
  stream.setInputStream(channel.open());

  let data = "";
  while (true) {
    let available = stream.available();
    if (available <= 0)
      break;
    data += stream.readBytes(available);
  }
  stream.close();

  return data;
}

// Create a temporary file from a given string and returns its path
exports.createFromString = function createFromString(data, tmpName) {  
  let filename = (tmpName ? tmpName : "tmp-file") + "-" + (new Date().getTime());
  let path = file.join(tmpDir, filename);

  let tmpFile = file.open(path, "wb");
  tmpFile.write(data);
  tmpFile.close();

  // Register tmp file path
  files.push(path);

  return path;
}

// Create a temporary file from a given URL and returns its path
exports.createFromURL = function createFromURL(url, tmpName) {
  let data = readBinaryURI(url);
  return exports.createFromString(data, tmpName);
}

