/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const MDN_BASE_URL =
  "https://firefox-source-docs.mozilla.org/devtools-user/storage_inspector/";

/**
 * Get the MDN URL for the specified storage type.
 *
 * @param {string} type Type of the storage.
 *
 * @return {string} The MDN URL for the storage type, or null if not available.
 */
function getStorageTypeURL(type) {
  switch (type) {
    case "cookies":
      return `${MDN_BASE_URL}cookies`;
    case "localStorage":
    case "sessionStorage":
      return `${MDN_BASE_URL}local_storage_session_storage`;
    case "indexedDB":
      return `${MDN_BASE_URL}indexeddb`;
    case "Cache":
      return `${MDN_BASE_URL}cache_storage`;
    case "extensionStorage":
      return `${MDN_BASE_URL}extension_storage`;
    default:
      return null;
  }
}

module.exports = getStorageTypeURL;
