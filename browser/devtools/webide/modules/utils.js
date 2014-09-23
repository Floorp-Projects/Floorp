/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cc, Cu, Ci } = require("chrome");
const { FileUtils } = Cu.import("resource://gre/modules/FileUtils.jsm", {});
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
const Strings = Services.strings.createBundle("chrome://browser/locale/devtools/webide.properties");

function doesFileExist (location) {
  let file = new FileUtils.File(location);
  return file.exists();
}
exports.doesFileExist = doesFileExist;

function getPackagedDirectory (window, location) {
  let directory;
  if (!location) {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, Strings.GetStringFromName("importPackagedApp_title"), Ci.nsIFilePicker.modeGetFolder);
    let res = fp.show();
    if (res == Ci.nsIFilePicker.returnCancel) {
      return null;
    }
    return fp.file;
  } else {
    return new FileUtils.File(location);
  }
}
exports.getPackagedDirectory = getPackagedDirectory;

function getHostedURL (window, location) {
  let ret = { value: null };

  if (!location) {
    Services.prompt.prompt(window,
        Strings.GetStringFromName("importHostedApp_title"),
        Strings.GetStringFromName("importHostedApp_header"),
        ret, null, {});
    location = ret.value;
  }

  if (!location) {
    return null;
  }

  // Clean location string and add "http://" if missing
  location = location.trim();
  try { // Will fail if no scheme
    Services.io.extractScheme(location);
  } catch(e) {
    location = "http://" + location;
  }
  return location;
}
exports.getHostedURL = getHostedURL;
