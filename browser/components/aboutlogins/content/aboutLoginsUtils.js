/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Dispatches a custom event to the AboutLoginsChild.jsm script which
 * will record the event.
 * @params {object} event.method The telemety event method
 * @params {object} event.object The telemety event object
 * @params {object} event.value [optional] The telemety event value
 */
export function recordTelemetryEvent(event) {
  document.dispatchEvent(
    new CustomEvent("AboutLoginsRecordTelemetryEvent", {
      bubbles: true,
      detail: event,
    })
  );
}
