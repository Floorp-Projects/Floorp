/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use-strict";

/**
 * Hamropatro's oauth logins with Google and Facebook redirect through a
 * different subdomain, and so require the use of the Storage Access API
 * for dFPI. This shim calls requestStorageAccess on behalf of the site
 * when a user logs in using Google or Facebook.
 */

// Origin we need to request storage access for.
const STORAGE_ACCESS_ORIGIN = "https://hamropatro.firebaseapp.com";

console.warn(
  `When using oauth, Firefox calls the Storage Access API on behalf of the site. See https://bugzilla.mozilla.org/show_bug.cgi?id=1660446 for details.`
);

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

    const buttonText = button.innerText?.toLowerCase();
    if (buttonText?.includes("facebook") || buttonText?.includes("google")) {
      button.disabled = true;
      button.style.opacity = 0.5;
      e.stopPropagation();
      e.preventDefault();
      document
        .requestStorageAccessForOrigin(STORAGE_ACCESS_ORIGIN)
        .then(() => {
          target.click();
        })
        .catch(() => {
          button.disabled = false;
          button.style.opacity = 1.0;
        });
    }
  },
  true
);
