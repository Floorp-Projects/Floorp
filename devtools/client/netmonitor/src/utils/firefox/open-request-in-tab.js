/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable mozilla/reject-some-requires */

"use strict";

let { Cc, Ci } = require("chrome");
const Services = require("Services");
const { gDevTools } = require("devtools/client/framework/devtools");

/**
 * Opens given request in a new tab.
 */
function openRequestInTab(request) {
  let win = Services.wm.getMostRecentWindow(gDevTools.chromeWindowType);
  let rawData = request.requestPostData ? request.requestPostData.postData : null;
  let postData;

  if (rawData && rawData.text) {
    let stringStream = getInputStreamFromString(rawData.text);
    postData = Cc["@mozilla.org/network/mime-input-stream;1"]
      .createInstance(Ci.nsIMIMEInputStream);
    postData.addHeader("Content-Type", "application/x-www-form-urlencoded");
    postData.setData(stringStream);
  }

  win.gBrowser.selectedTab = win.gBrowser.addTab(request.url, null, null, postData);
}

function getInputStreamFromString(data) {
  let stringStream = Cc["@mozilla.org/io/string-input-stream;1"]
    .createInstance(Ci.nsIStringInputStream);
  stringStream.data = data;
  return stringStream;
}

module.exports = {
  openRequestInTab,
};
