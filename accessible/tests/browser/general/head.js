/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported initAccessibilityService, openNewTab, shutdownAccessibilityService */

// Load the shared-head file first.
/* import-globals-from ../shared-head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js",
  this);

const nsIAccessibleRole = Ci.nsIAccessibleRole; // eslint-disable-line no-unused-vars

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

async function openNewTab(url) {
  const forceNewProcess = true;

  return BrowserTestUtils.openNewForegroundTab(
    { gBrowser, url, forceNewProcess });
}

async function initAccessibilityService() {
  info("Create accessibility service.");
  let accService = Cc["@mozilla.org/accessibilityService;1"].getService(
    Ci.nsIAccessibilityService);

  await new Promise(resolve => {
    if (Services.appinfo.accessibilityEnabled) {
      resolve();
      return;
    }

    let observe = (subject, topic, data) => {
      if (data === "1") {
        Services.obs.removeObserver(observe, "a11y-init-or-shutdown");
        resolve();
      }
    };
    Services.obs.addObserver(observe, "a11y-init-or-shutdown");
  });

  return accService;
}

function shutdownAccessibilityService() {
  forceGC();

  return new Promise(resolve => {
    if (!Services.appinfo.accessibilityEnabled) {
      resolve();
      return;
    }

    let observe = (subject, topic, data) => {
      if (data === "0") {
        Services.obs.removeObserver(observe, "a11y-init-or-shutdown");
        resolve();
      }
    };
    Services.obs.addObserver(observe, "a11y-init-or-shutdown");
  });
}
