/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is a chrome-API-free version of the module
// devtools/client/netmonitor/src/utils/firefox/open-request-in-tab.js, so that
// it can be used in Chrome-API-free applications, such as the Launchpad. But
// because of this, it cannot take advantage of utilizing chrome APIs and should
// implement the similar functionalities on its own.
//
// Please keep in mind that if the feature in this file has changed, don't
// forget to also change that accordingly in
// devtools/client/netmonitor/src/utils/firefox/open-request-in-tab.js.

"use strict";

const Services = require("Services");
const { gDevTools } = require("devtools/client/framework/devtools");

/**
 * Opens given request in a new tab.
 */
function openRequestInTab(url, requestPostData) {
  let win = Services.wm.getMostRecentWindow(gDevTools.chromeWindowType);
  if (!requestPostData) {
    win.openWebLinkIn(url, "tab", {relatedToCurrent: true});
  } else {
    openPostRequestInTabHelper({
      url,
      data: requestPostData.postData
    });
  }
}

function openPostRequestInTabHelper({url, data}) {
  let form = document.createElement("form");
  form.target = "_blank";
  form.action = url;
  form.method = "post";

  if (data) {
    for (let key in data) {
      let input = document.createElement("input");
      input.name = key;
      input.value = data[key];
      form.appendChild(input);
    }
  }

  form.hidden = true;
  document.body.appendChild(form);
  form.submit();
  form.remove();
}

module.exports = {
  openRequestInTab,
};
