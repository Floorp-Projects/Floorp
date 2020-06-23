"use strict";

/**
 * Bug 1641998 - Embedded Twitter videos - Override indexedDB API
 * See also: https://bugzilla.mozilla.org/show_bug.cgi?id=1641521
 *
 * With ETP enabled, embedded Twitter videos break. This is caused by Twitter
 * trying to access the indexedDB API for testing if the API is there, without
 * anticipating the potential for a SecurityError to be thrown.
 *
 * This site patch sets window.indexedDB to undefined so their detection can
 * work correctly.
 */

/* globals exportFunction */

console.info(
  "window.indexedDB has been overwritten for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1641521 for details."
);

Object.defineProperty(window.wrappedJSObject, "indexedDB", {
  get: undefined,
  set: undefined,
});
