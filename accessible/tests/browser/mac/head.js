/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported getNativeInterface, waitForMacEventWithInfo, waitForMacEvent, waitForStateChange,
   NSRange, NSDictionary, stringForRange, AXTextStateChangeTypeEdit,
   AXTextEditTypeDelete, AXTextEditTypeTyping, AXTextStateChangeTypeSelectionMove,
   AXTextStateChangeTypeSelectionExtend, AXTextSelectionDirectionUnknown,
   AXTextSelectionDirectionPrevious, AXTextSelectionDirectionNext,
   AXTextSelectionDirectionDiscontiguous, AXTextSelectionGranularityUnknown,
   AXTextSelectionGranularityCharacter, AXTextSelectionGranularityWord */

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

// AXTextStateChangeType enum values
const AXTextStateChangeTypeEdit = 1;
const AXTextStateChangeTypeSelectionMove = 2;
const AXTextStateChangeTypeSelectionExtend = 3;

// AXTextEditType enum values
const AXTextEditTypeDelete = 1;
const AXTextEditTypeTyping = 3;

// AXTextSelectionDirection enum values
const AXTextSelectionDirectionUnknown = 0;
const AXTextSelectionDirectionPrevious = 3;
const AXTextSelectionDirectionNext = 4;
const AXTextSelectionDirectionDiscontiguous = 5;

// AXTextSelectionGranularity enum values
const AXTextSelectionGranularityUnknown = 0;
const AXTextSelectionGranularityCharacter = 1;
const AXTextSelectionGranularityWord = 2;

function getNativeInterface(accDoc, id) {
  return findAccessibleChildByID(accDoc, id).nativeInterface.QueryInterface(
    Ci.nsIAccessibleMacInterface
  );
}

function waitForMacEventWithInfo(notificationType, filter) {
  let filterFunc = (macIface, data) => {
    if (!filter) {
      return true;
    }

    if (typeof filter == "function") {
      return filter(macIface, data);
    }

    return macIface.getAttributeValue("AXDOMIdentifier") == filter;
  };

  return new Promise(resolve => {
    let eventObserver = {
      observe(subject, topic, data) {
        let macEvent = subject.QueryInterface(Ci.nsIAccessibleMacEvent);
        if (
          data === notificationType &&
          filterFunc(macEvent.macIface, macEvent.data)
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

  let str = macDoc.getParameterizedAttributeValue(
    "AXStringForTextMarkerRange",
    range
  );

  let attrStr = macDoc.getParameterizedAttributeValue(
    "AXAttributedStringForTextMarkerRange",
    range
  );

  // This is a fly-by test to make sure our attributed strings
  // always match our flat strings.
  is(
    attrStr.map(({ string }) => string).join(""),
    str,
    "attributed text matches non-attributed text"
  );

  return str;
}

function waitForStateChange(id, state, isEnabled) {
  return waitForEvent(EVENT_STATE_CHANGE, e => {
    e.QueryInterface(nsIAccessibleStateChangeEvent);
    return (
      e.state == state &&
      !e.isExtraState &&
      isEnabled == e.isEnabled &&
      id == getAccessibleDOMNodeID(e.accessible)
    );
  });
}
