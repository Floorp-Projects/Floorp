/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

function isLeftClick(event, allowKeyModifiers) {
  return event.button === 0 && (allowKeyModifiers || noKeyModifiers(event));
}

function noKeyModifiers(event) {
  return !event.ctrlKey && !event.shiftKey && !event.altKey && !event.metaKey;
}

function cancelEvent(event) {
  event.stopPropagation();
  event.preventDefault();
}

// Exports from this module
exports.isLeftClick = isLeftClick;
exports.cancelEvent = cancelEvent;
