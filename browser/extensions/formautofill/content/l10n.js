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

const CONTENT_WIN = typeof(window) != "undefined" ? window : this;

const L10N_ATTRIBUTES = ["data-localization", "data-localization-region"];

// eslint-disable-next-line mozilla/balanced-listeners
CONTENT_WIN.addEventListener("DOMContentLoaded", function onDCL(evt) {
  let doc = evt.target;
  FormAutofillUtils.localizeMarkup(doc);

  let mutationObserver = new doc.ownerGlobal.MutationObserver(function onMutation(mutations) {
    for (let mutation of mutations) {
      if (!mutation.target.hasAttribute(mutation.attributeName)) {
        // The attribute was removed in the meantime.
        continue;
      }
      FormAutofillUtils.localizeAttributeForElement(mutation.target, mutation.attributeName);
    }
  });

  mutationObserver.observe(doc, {
    attributes: true,
    attributeFilter: L10N_ATTRIBUTES,
    subtree: true,
  });
});
