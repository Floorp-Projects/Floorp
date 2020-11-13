/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported getNativeInterface, waitForMacEventWithInfo, waitForMacEvent,
   NSRange, NSDictionary, stringForRange */

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

function waitForMacEventWithInfo(notificationType, filter) {
  return new Promise(resolve => {
    let eventObserver = {
      observe(subject, topic, data) {
        let macEvent = subject.QueryInterface(Ci.nsIAccessibleMacEvent);
        if (
          data === notificationType &&
          (!filter || filter(macEvent.macIface, macEvent.data))
        ) {
          Services.obs.removeObserver(this, "accessible-mac-event");
          resolve(macEvent);
        }
      },
    };
    Services.obs.addObserver(eventObserver, "accessible-mac-event");
  });
}

function waitForMacEvent(notificationType, filter) {
  return waitForMacEventWithInfo(notificationType, filter).then(
    e => e.macIface
  );
}

function NSRange(location, length) {
  return {
    valueType: "NSRange",
    value: [location, length],
  };
}

function NSDictionary(dict) {
  return {
    objectType: "NSDictionary",
    object: dict,
  };
}

function stringForRange(macDoc, range) {
  if (!range) {
    return "";
  }

  return macDoc.getParameterizedAttributeValue(
    "AXStringForTextMarkerRange",
    range
  );
}
