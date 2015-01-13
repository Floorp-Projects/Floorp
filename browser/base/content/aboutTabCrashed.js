/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function parseQueryString() {
  let url = document.documentURI;
  let queryString = url.replace(/^about:tabcrashed?e=tabcrashed/, "");

  let titleMatch = queryString.match(/d=([^&]*)/);
  return titleMatch && titleMatch[1] ? decodeURIComponent(titleMatch[1]) : "";
}

document.title = parseQueryString();

function shouldSendReport() {
  if (!document.documentElement.classList.contains("crashDumpAvailable"))
    return false;
  return document.getElementById("sendReport").checked;
}

function sendEvent(message) {
  let event = new CustomEvent("AboutTabCrashedMessage", {
    bubbles: true,
    detail: {
      message,
      sendCrashReport: shouldSendReport(),
    },
  });

  document.dispatchEvent(event);
}

function closeTab() {
  sendEvent("closeTab");
}

function restoreTab() {
  sendEvent("restoreTab");
}

function restoreAll() {
  sendEvent("restoreAll");
}

// Error pages are loaded as LOAD_BACKGROUND, so they don't get load events.
var event = new CustomEvent("AboutTabCrashedLoad", {bubbles:true});
document.dispatchEvent(event);
