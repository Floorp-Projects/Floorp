/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Bug 1802340 - tsn.ca login broken with dFPI enabled
 *
 * tsn.ca relies upon a login page that is out-of-origin. That login page
 * sets a cookie for https://www.tsn.ca, which is then used as an proof of
 * authentication on redirect back to the main site. This shim adds a request
 * for storage access for https://www.tsn.ca when the user tries to log in.
 */

console.warn(
  `When logging in, Firefox calls the Storage Access API on behalf of the site. See https://bugzilla.mozilla.org/show_bug.cgi?id=1802340 for details.`
);

// Third-party origin we need to request storage access for.
const STORAGE_ACCESS_ORIGIN = "https://www.tsn.ca";

document.documentElement.addEventListener(
  "click",
  e => {
    const { target, isTrusted } = e;
    if (!isTrusted) {
      return;
    }

    const button = target.closest("button");
    if (!button) {
      return;
    }
    const form = target.closest(".login-form");
    if (!form) {
      return;
    }

    console.warn(
      "Calling the Storage Access API on behalf of " + STORAGE_ACCESS_ORIGIN
    );
    button.disabled = true;
    e.stopPropagation();
    e.preventDefault();
    document
      .requestStorageAccessForOrigin(STORAGE_ACCESS_ORIGIN)
      .then(() => {
        button.disabled = false;
        target.click();
      })
      .catch(() => {
        button.disabled = false;
      });
  },
  true
);
