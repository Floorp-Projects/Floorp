/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const CONCEALED_PASSWORD_TEXT = " ".repeat(8);

/**
 * Dispatches a custom event to the AboutLoginsChild.sys.mjs script which
 * will record the event.
 * @param {object} event.method The telemety event method
 * @param {object} event.object The telemety event object
 * @param {object} event.value [optional] The telemety event value
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

  let { activeElement: docActiveElement } = document;
  if (
    !enableKeyboardAccess &&
    docActiveElement &&
    !docActiveElement.closest("confirmation-dialog")
  ) {
    let elementToBlur =
      docActiveElement?.shadowRoot?.activeElement ?? docActiveElement;
    elementToBlur.blur();
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

export function promptForPrimaryPassword(messageId) {
  return new Promise(resolve => {
    window.AboutLoginsUtils.promptForPrimaryPassword(resolve, messageId);
  });
}

/**
 * Initializes a dialog based on a template using shadow dom.
 * @param {HTMLElement} element The element to attach the shadow dom to.
 * @param {string} templateSelector The selector of the template to be used.
 * @returns {object} The shadow dom that is attached.
 */
export function initDialog(element, templateSelector) {
  let template = document.querySelector(templateSelector);
  let shadowRoot = element.attachShadow({ mode: "open" });
  document.l10n.connectRoot(shadowRoot);
  shadowRoot.appendChild(template.content.cloneNode(true));
  return shadowRoot;
}
