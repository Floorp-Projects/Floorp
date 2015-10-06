/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function parseQueryString() {
  let URL = document.documentURI;
  let queryString = URL.replace(/^about:tabcrashed?e=tabcrashed/, "");

  let titleMatch = queryString.match(/d=([^&]*)/);
  let URLMatch = queryString.match(/u=([^&]*)/);
  return {
    title: titleMatch && titleMatch[1] ? decodeURIComponent(titleMatch[1]) : "",
    URL: URLMatch && URLMatch[1] ? decodeURIComponent(URLMatch[1]) : "",
  };
}

function displayUI() {
  if (!hasReport()) {
    return;
  }

  let sendCrashReport = document.getElementById("sendReport").checked;
  let container = document.getElementById("crash-reporter-container");
  container.hidden = !sendCrashReport;
}

function hasReport() {
  return document.documentElement.classList.contains("crashDumpAvailable");
}

function sendEvent(message) {
  let comments = "";
  let email = "";
  let URL = "";
  let sendCrashReport = false;
  let emailMe = false;
  let includeURL = false;

  if (hasReport()) {
    sendCrashReport = document.getElementById("sendReport").checked;
    if (sendCrashReport) {
      comments = document.getElementById("comments").value.trim();

      includeURL = document.getElementById("includeURL").checked;
      if (includeURL) {
        URL = parseQueryString().URL.trim();
      }

      emailMe = document.getElementById("emailMe").checked;
      if (emailMe) {
        email = document.getElementById("email").value.trim();
      }
    }
  }

  let event = new CustomEvent("AboutTabCrashedMessage", {
    bubbles: true,
    detail: {
      message,
      sendCrashReport,
      comments,
      email,
      emailMe,
      includeURL,
      URL,
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

document.title = parseQueryString().title;

// Error pages are loaded as LOAD_BACKGROUND, so they don't get load events.
var event = new CustomEvent("AboutTabCrashedLoad", {bubbles:true});
document.dispatchEvent(event);

addEventListener("DOMContentLoaded", function() {
  let sendReport = document.getElementById("sendReport");
  sendReport.addEventListener("click", function() {
    displayUI();
  });

  displayUI();
});
