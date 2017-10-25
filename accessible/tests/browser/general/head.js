/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported initAccessibilityService, shutdownAccessibilityService */

// Load the shared-head file first.
/* import-globals-from ../shared-head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js",
  this);

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
