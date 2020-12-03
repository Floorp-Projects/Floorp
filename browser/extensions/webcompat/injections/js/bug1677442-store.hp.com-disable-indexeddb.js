"use strict";

/**
 * Bug 1677442 - disable indexedDB for d3nkfb7815bs43.cloudfront.net
 *
 * The site embeds an iframe with a 3D viewer. The request fails
 *  because BabylonJS (the 3d library) tries to access indexedDB
 *  from the third party context (d3nkfb7815bs43.cloudfront.net)
 *  Disabling indexedDB fixes it, causing it to fetch the 3d resource
 *  via network.
 */

console.info(
  "window.indexedDB has been overwritten for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1677442 for details."
);

Object.defineProperty(window.wrappedJSObject, "indexedDB", {
  get: undefined,
  set: undefined,
});
