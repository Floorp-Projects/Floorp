/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;

const {NetUtil} = Cu.import("resource://gre/modules/NetUtil.jsm", {});
const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});

this.EXPORTED_SYMBOLS = ["DevToolsPreferences"];

// Synchronously fetch the content of a given URL
function readURI(uri) {
  let stream = NetUtil.newChannel({
    uri: NetUtil.newURI(uri, "UTF-8"),
    loadUsingSystemPrincipal: true}
  ).open2();
  let count = stream.available();
  let data = NetUtil.readInputStreamToString(stream, count, {
    charset: "UTF-8"
  });

  stream.close();

  return data;
}

/**
 * Cleanup the preferences file content from all empty and comment lines.
 *
 * @param  {String} content
 *         The string content of a preferences file.
 * @return {String} the content stripped of unnecessary lines.
 */
function cleanupPreferencesFileContent(content) {
  let lines = content.split("\n");
  let newLines = [];
  let continuation = false;
  for (let line of lines) {
    let isPrefLine = /^ *pref\("([^"]+)"/.test(line);
    if (continuation || isPrefLine) {
      newLines.push(line);
      // The call to pref(...); might span more than one line.
      continuation = !/\);/.test(line);
    }
  }
  return newLines.join("\n");
}

// Read a preference file and set all of its defined pref as default values
// (This replicates the behavior of preferences files from mozilla-central)
function processPrefFile(url) {
  let content = readURI(url);
  content = cleanupPreferencesFileContent(content);
  content.match(/pref\("[^"]+",\s*.+\s*\)/g).forEach(item => {
    let m = item.match(/pref\("([^"]+)",\s*(.+)\s*\)/);
    let name = m[1];
    let val = m[2].trim();

    // Prevent overriding prefs that have been changed by the user
    if (Services.prefs.prefHasUserValue(name)) {
      return;
    }
    let defaultBranch = Services.prefs.getDefaultBranch("");
    if ((val.startsWith("\"") && val.endsWith("\"")) ||
        (val.startsWith("'") && val.endsWith("'"))) {
      val = val.substr(1, val.length - 2);
      val = val.replace(/\\"/g, '"');
      defaultBranch.setCharPref(name, val);
    } else if (val.match(/[0-9]+/)) {
      defaultBranch.setIntPref(name, parseInt(val, 10));
    } else if (val == "true" || val == "false") {
      defaultBranch.setBoolPref(name, val == "true");
    } else {
      console.log("Unable to match preference type for value:", val);
    }
  });
}

this.DevToolsPreferences = {
  loadPrefs: function () {
    processPrefFile("chrome://devtools/content/preferences/devtools.js");
    processPrefFile("chrome://devtools/content/preferences/debugger.js");
    processPrefFile("chrome://devtools/content/webide/webide-prefs.js");
  }
};
