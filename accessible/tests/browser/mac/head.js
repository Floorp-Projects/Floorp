/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported getNativeInterface, waitForMacEvent */

// Load the shared-head file first.
/* import-globals-from ../shared-head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js",
  this
);

// Loading and common.js from accessible/tests/mochitest/ for all tests, as
// well as promisified-events.js.
loadScripts(
  { name: "common.js", dir: MOCHITESTS_DIR },
  { name: "promisified-events.js", dir: MOCHITESTS_DIR }
);

function getNativeInterface(accDoc, id) {
  return findAccessibleChildByID(accDoc, id).nativeInterface.QueryInterface(
    Ci.nsIAccessibleMacInterface
  );
}

function waitForMacEvent(notificationType, filter) {
  return new Promise(resolve => {
    let eventObserver = {
      observe(subject, topic, data) {
        let macIface = subject.QueryInterface(Ci.nsIAccessibleMacInterface);
        if (data === notificationType && (!filter || filter(macIface))) {
          Services.obs.removeObserver(this, "accessible-mac-event");
          resolve(macIface);
        }
      },
    };
    Services.obs.addObserver(eventObserver, "accessible-mac-event");
  });
}
