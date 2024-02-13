/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals exportFunction */

"use strict";

/**
 * Blogger powered blogs rely on storage access to https://blogger.com to enable
 * oauth with Google. For dFPI, sites need to use the Storage Access API to gain
 * first party storage access. This shim calls requestStorageAccess on behalf of
 * the site when a user wants to log in via oauth.
 */

console.warn(
  `When using oauth, Firefox calls the Storage Access API on behalf of the site. See https://bugzilla.mozilla.org/show_bug.cgi?id=1776869 for details.`
);

const GOOGLE_OAUTH_PATH_PREFIX = "https://accounts.google.com/ServiceLogin";

// After permission was granted request (use) storage access and reload
async function requestGrantedAccess() {
  const storageAccessPermission = await navigator.permissions.query({
    name: "storage-access",
  });
  const hasStorageAccess = await document.hasStorageAccess();
  if (storageAccessPermission.state === "granted" && !hasStorageAccess) {
    await document.requestStorageAccess();
    location.reload();
  }
}

requestGrantedAccess();

// Overwrite the window.open method so we can detect oauth related popups.
const origOpen = window.wrappedJSObject.open;
Object.defineProperty(window.wrappedJSObject, "open", {
  value: exportFunction((url, ...args) => {
    // Filter oauth popups.
    if (!url.startsWith(GOOGLE_OAUTH_PATH_PREFIX)) {
      return origOpen(url, ...args);
    }
    // Request storage access for the Blogger iframe.
    document.requestStorageAccess().then(() => {
      origOpen(url, ...args);
    });
    // We don't have the window object yet which window.open returns, since the
    // sign-in flow is dependent on the async storage access request. This isn't
    // a problem as long as the website does not consume it.
    return null;
  }, window),
});
