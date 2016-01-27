/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/NetUtil.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");

waitForExplicitFinish();

const PAGE = "data:text/html,<html><body><input type=\"file\"/></body></html>";

function writeFile(file, text) {
  return new Promise((resolve) => {
    let converter = Components.classes["@mozilla.org/intl/scriptableunicodeconverter"]
                              .createInstance(Components.interfaces.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    let ostream = FileUtils.openSafeFileOutputStream(file);
    let istream = converter.convertToInputStream(text);
    NetUtil.asyncCopy(istream, ostream, function(status) {
      if (!Components.isSuccessCode(status)) throw 'fail';
      resolve();
    });
  });
}

function runFileReader(input, status) {
  return new Promise((resolve) => {
    let fr = new FileReader();
    fr.onload = function() {
      ok(status, "FileReader called onload");
      resolve();
    }

    fr.onerror = function(e) {
      e.preventDefault();
      ok(!status, "FileReader called onerror");
      resolve();
    }

    fr.readAsArrayBuffer(input);
  });
}

add_task(function() {
  info("Creating a temporary file...");
  let file = FileUtils.getFile("TmpD", ["bug1238440.txt"]);
  yield writeFile(file, "hello world");

  info("Opening a tab...");
  let tab = gBrowser.addTab(PAGE);
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;
  yield BrowserTestUtils.browserLoaded(browser);

  info("Populating the form...");
  let doc = browser.contentDocument;
  let input = doc.querySelector('input');
  input.value = file.path;

  info("Running the FileReader...");
  yield runFileReader(input.files[0], true);

  info("Writing the temporary file again...");
  yield writeFile(file, "hello world-----------------------------");

  info("Running the FileReader again...");
  yield runFileReader(input.files[0], false);

  info("Closing the tab...");
  gBrowser.removeTab(gBrowser.selectedTab);

  ok(true, "we didn't crash.");
});
