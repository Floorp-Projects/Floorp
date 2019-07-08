/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/client/shared/browser-loader.js"
);
const { require } = BrowserLoader({
  baseURI: "resource://devtools/client/aboutdebugging-new/",
  window,
});

// The only purpose of this module is to load the real aboutdebugging module via the
// BrowserLoader.
// This cannot be done using an inline script tag in index.html because we are applying
// CSP for about: pages in Bug 1492063.
// And this module cannot be merged with aboutdebugging.js because modules loaded with
// script tags are using Promises bound to the lifecycle of the document, while modules
// loaded with a devtools loader use Promises that will still resolve if the document is
// destroyed. This is particularly useful to ensure asynchronous destroy() calls succeed.
require("./aboutdebugging");
