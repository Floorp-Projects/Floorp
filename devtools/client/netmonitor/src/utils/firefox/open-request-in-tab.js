/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable mozilla/reject-some-requires */

// This file is a chrome-API-dependent version of the module
// devtools/client/netmonitor/src/utils/open-request-in-tab.js, so that it can
// take advantage of utilizing chrome APIs. But because of this, it isn't
// intended to be used in Chrome-API-free applications, such as the Launchpad.
//
// Please keep in mind that if the feature in this file has changed, don't
// forget to also change that accordingly in
// devtools/client/netmonitor/src/utils/open-request-in-tab.js.

"use strict";

const { Cc, Ci } = require("chrome");
const Services = require("Services");
const { gDevTools } = require("devtools/client/framework/devtools");

/**
 * Opens given request in a new tab.
 */
function openRequestInTab(url, requestHeaders, requestPostData) {
  const win = Services.wm.getMostRecentWindow(gDevTools.chromeWindowType);
  const rawData = requestPostData ? requestPostData.postData : null;
  let postData;

  if (rawData?.text) {
    const stringStream = getInputStreamFromString(rawData.text);
    postData = Cc["@mozilla.org/network/mime-input-stream;1"].createInstance(
      Ci.nsIMIMEInputStream
    );

    const contentTypeHeader = requestHeaders.headers.find(e => {
      return e.name.toLowerCase() === "content-type";
    });

    postData.addHeader(
      "Content-Type",
      contentTypeHeader
        ? contentTypeHeader.value
        : "application/x-www-form-urlencoded"
    );
    postData.setData(stringStream);
  }
  const { userContextId } = win.gBrowser.contentPrincipal;
  win.gBrowser.selectedTab = win.gBrowser.addWebTab(url, {
    // TODO this should be using the original request principal
    triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({
      userContextId,
    }),
    userContextId,
    postData,
  });
}

function getInputStreamFromString(data) {
  const stringStream = Cc[
    "@mozilla.org/io/string-input-stream;1"
  ].createInstance(Ci.nsIStringInputStream);
  stringStream.data = data;
  return stringStream;
}

module.exports = {
  openRequestInTab,
};
