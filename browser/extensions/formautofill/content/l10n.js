/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This file will be replaced by Fluent but it's a middle ground so we can share
 * the edit dialog code with the unprivileged PaymentRequest dialog before the
 * Fluent conversion
 */

/* global content */

ChromeUtils.import("resource://formautofill/FormAutofillUtils.jsm");

const CONTENT_WIN = typeof(window) != "undefined" ? window : content.window;

const L10N_ATTRIBUTES = ["data-localization", "data-localization-region"];

let mutationObserver = new CONTENT_WIN.MutationObserver(function onMutation(mutations) {
  for (let mutation of mutations) {
    if (!mutation.target.hasAttribute(mutation.attributeName)) {
      // The attribute was removed in the meantime.
      continue;
    }
    FormAutofillUtils.localizeAttributeForElement(mutation.target, mutation.attributeName);
  }
});

CONTENT_WIN.document.addEventListener("DOMContentLoaded", function onDCL() {
  FormAutofillUtils.localizeMarkup(CONTENT_WIN.document);
  mutationObserver.observe(CONTENT_WIN.document, {
    attributes: true,
    attributeFilter: L10N_ATTRIBUTES,
    subtree: true,
  });
}, {
  once: true,
});
