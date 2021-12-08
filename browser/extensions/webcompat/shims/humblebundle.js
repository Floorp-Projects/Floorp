/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Humblebundle's oauth logins with Google and Facebook redirect through a
 * different subdomain, and so require the use of the Storage Access API
 * for dFPI. This shim calls requestStorageAccess on behalf of the site
 * when a user logs in using Google or Facebook.
 */

// Origin we need to request storage access for.
const STORAGE_ACCESS_ORIGIN = "https://hr-humblebundle.firebaseapp.com";

console.warn(
  `When using oauth, Firefox calls the Storage Access API on behalf of the site. See https://bugzilla.mozilla.org/show_bug.cgi?id=1742553 for details.`
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

    if (
      button.classList.contains("js-facebook-ssi") ||
      button.classList.contains("js-google-ssi")
    ) {
      // Disable the button to prevent additional clicks while the storage
      // access prompt is open or during the auto-grant period.
      button.disabled = true;
      button.style.opacity = 0.5;
      e.stopPropagation();
      e.preventDefault();
      document
        .requestStorageAccessForOrigin(STORAGE_ACCESS_ORIGIN)
        .then(() => {
          // Enable the button again so the click goes through.
          button.disabled = false;
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
