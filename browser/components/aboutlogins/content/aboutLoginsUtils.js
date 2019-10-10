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

export function setKeyboardAccessForNonDialogElements(enableKeyboardAccess) {
  const pageElements = document.querySelectorAll(
    "login-item, login-list, menu-button, login-filter, fxaccounts-button, [tabindex]"
  );

  if (
    !enableKeyboardAccess &&
    document.activeElement &&
    !document.activeElement.closest("confirmation-dialog")
  ) {
    let { activeElement } = document;
    if (activeElement.shadowRoot && activeElement.shadowRoot.activeElement) {
      activeElement.shadowRoot.activeElement.blur();
    } else {
      document.activeElement.blur();
    }
  }

  pageElements.forEach(el => {
    if (!enableKeyboardAccess) {
      if (el.tabIndex > -1) {
        el.dataset.oldTabIndex = el.tabIndex;
      }
      el.tabIndex = "-1";
    } else if (el.dataset.oldTabIndex) {
      el.tabIndex = el.dataset.oldTabIndex;
      delete el.dataset.oldTabIndex;
    } else {
      el.removeAttribute("tabindex");
    }
  });
}
