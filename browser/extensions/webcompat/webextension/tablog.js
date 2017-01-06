/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const consoleLogScript = 'console.log("The user agent has been overridden for compatibility reasons.");';

/* This function handle tab updates.
 * While changeInfo.status is loading and there is url
 * Send tab url to ua_overrider.js to query if needs to print web console log.
 */
function handleUpdated(tabId, changeInfo, tabInfo) {
  if (changeInfo.status === "loading" && changeInfo.url) {
    chrome.runtime.sendMessage({url: changeInfo.url}, (message) => {
      if (message.reply) {
        chrome.tabs.executeScript(tabId, {code: consoleLogScript});
      }
    });
  }
}

chrome.tabs.onUpdated.addListener(handleUpdated);
