/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global window, document */

"use strict";

const React = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { appinfo } = require("Services");

const { buildFakeToolbox, Inspector } = require("./inspector");

function onConnect(arg) {
  if (!arg || !arg.client) {
    return;
  }

  let client = arg.client;

  const tabTarget = client.getTabTarget();
  let threadClient = { paused: false };
  buildFakeToolbox(
    tabTarget,
    () => threadClient,
    { React, ReactDOM, browserRequire: () => {} }
  ).then(function (fakeToolbox) {
    let inspector = new Inspector(fakeToolbox);
    inspector.init();
  });
}

/**
 * Stylesheet links in devtools xhtml files are using chrome or resource URLs.
 * Rewrite the href attribute to remove the protocol. web-server.js contains redirects
 * to map CSS urls to the proper file. Supports urls using:
 *   - devtools/client/
 *   - devtools/content/
 *   - skin/
 * The css for the light-theme will additionally be loaded.
 * Will also add mandatory classnames and attributes to be compatible with devtools theme
 * stylesheet.
 *
 */
function fixStylesheets(doc) {
  let links = doc.head.querySelectorAll("link");
  for (let link of links) {
    link.href = link.href.replace(/(resource|chrome)\:\/\//, "/");
  }

  // Add the light theme stylesheet to compensate for the missing theme-switching.js
  let themeLink = doc.createElement("link");
  themeLink.setAttribute("rel", "stylesheet");
  themeLink.setAttribute("href", "/devtools/skin/light-theme.css");

  doc.head.appendChild(themeLink);
  doc.documentElement.classList.add("theme-light");
  doc.body.classList.add("theme-light");

  if (appinfo.OS === "Darwin") {
    doc.documentElement.setAttribute("platform", "mac");
  } else if (appinfo.OS === "Linux") {
    doc.documentElement.setAttribute("platform", "linux");
  } else {
    doc.documentElement.setAttribute("platform", "win");
  }
}

/**
 * Called each time a childList mutation is received on the main document.
 * Check the iframes currently loaded in the document and call fixStylesheets if needed.
 */
function fixStylesheetsOnMutation() {
  let frames = document.body.querySelectorAll("iframe");
  for (let frame of frames) {
    let doc = frame.contentDocument || frame.contentWindow.document;
    if (doc.__fixStylesheetsFlag) {
      continue;
    }

    // Mark the document as processed to avoid extra changes.
    doc.__fixStylesheetsFlag = true;
    if (doc.readyState !== "complete") {
      // If the document is not loaded yet, wait for DOMContentLoaded.
      frame.contentWindow.addEventListener("DOMContentLoaded", () => {
        fixStylesheets(doc);
      }, { once: true });
    } else {
      fixStylesheets(doc);
    }
  }
}

window.addEventListener("DOMContentLoaded", function () {
  // Add styling for the main document.
  fixStylesheets(document);

  // Add a mutation observer to check if new iframes have been loaded and need to have
  //  their stylesheet links updated.
  new window.MutationObserver(mutations => {
    fixStylesheetsOnMutation();
  }).observe(document.body, { childList: true, subtree: true });

  const hasFirefoxTabParam = window.location.href.indexOf("firefox-tab") != -1;
  if (!hasFirefoxTabParam) {
    const inspectorRoot = document.querySelector(".inspector");
    // Remove the inspector specific markup and add the landing page root element.
    inspectorRoot.remove();
    let mount = document.createElement("div");
    mount.setAttribute("id", "mount");
    document.body.appendChild(mount);
  }

  // Toolbox tries to add a theme classname on the documentElement and should only be
  // required after DOMContentLoaded.
  const { bootstrap } = require("devtools-launchpad");
  bootstrap(React, ReactDOM).then(onConnect);
}, {once: true});
