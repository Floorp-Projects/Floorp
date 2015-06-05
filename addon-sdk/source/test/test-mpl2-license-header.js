// Note: This line is here intentionally, to break MPL2_LICENSE_TEST
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu } = require("chrome");
const options = require('@loader/options');
const { id } = require("sdk/self");
const { getAddonByID } = require("sdk/addon/manager");
const { mapcat, map, filter, fromEnumerator } = require("sdk/util/sequence");
const { readURISync } = require('sdk/net/url');

const ios = Cc['@mozilla.org/network/io-service;1'].
              getService(Ci.nsIIOService);

const MIT_LICENSE_HEADER = [];

const MPL2_LICENSE_TEST = new RegExp([
  "^\\/\\* This Source Code Form is subject to the terms of the Mozilla Public",
  " \\* License, v\\. 2\\.0\\. If a copy of the MPL was not distributed with this",
  " \\* file, You can obtain one at http:\\/\\/mozilla\\.org\\/MPL\\/2\\.0\\/\\. \\*\\/"
].join("\n"));

// Note: Using regular expressions because the paths a different for cfx vs jpm
const IGNORES = [
  /lib[\/\\](diffpatcher|method)[\/\\].+$/, // MIT
  /lib[\/\\]sdk[\/\\]fs[\/\\]path\.js$/, // MIT
  /lib[\/\\]sdk[\/\\]system[\/\\]child_process[\/\\].*/,
  /tests?[\/\\]buffers[\/\\].+$/, // MIT
  /tests?[\/\\]path[\/\\]test-path\.js$/,
  /tests?[\/\\]querystring[\/\\]test-querystring\.js$/,
];

const ignoreFile = file => !!IGNORES.find(regex => regex.test(file));

const baseURI = "resource://test-sdk-addon/";

const uri = (path="") => baseURI + path;

const toFile = x => x.QueryInterface(Ci.nsIFile);
const isTestFile = ({ path, leafName }) => {
  return !ignoreFile(path) && /\.jsm?$/.test(leafName)
};
const getFileURI = x => ios.newFileURI(x).spec;

const getDirectoryEntries = file => map(toFile, fromEnumerator(_ => file.directoryEntries));

const isDirectory = x => x.isDirectory();
const getEntries = directory => mapcat(entry => {
  if (isDirectory(entry)) {
    return getEntries(entry);
  }
  else if (isTestFile(entry)) {
    return [ entry ];
  }
  return [];
}, filter(() => true, getDirectoryEntries(directory)));


exports["test MPL2 license header"] = function*(assert) {
  let addon = yield getAddonByID(id);
  let xpiURI = addon.getResourceURI();
  let rootURL = xpiURI.spec;
  let files = [...getEntries(xpiURI.QueryInterface(Ci.nsIFileURL).file)];

  assert.ok(files.length > 1, files.length + " files found.");
  let failures = [];
  let success = 0;

  files.forEach(file => {
    const URI = ios.newFileURI(file);
    let leafName = URI.spec.replace(rootURL, "");
    let contents = readURISync(URI);

    if (!MPL2_LICENSE_TEST.test(contents)) {
      failures.push(leafName);
    }
  });

  assert.equal(1, failures.length, "we expect one failure");
  assert.ok(/test-mpl2-license-header\.js$/.test(failures[0]), "the only failure is this file");
  failures.shift();
  assert.equal("", failures.join(",\n"), failures.length + " files found missing the required mpl 2 header");
}

require("sdk/test").run(exports);
