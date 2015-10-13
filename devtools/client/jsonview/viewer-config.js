/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * RequireJS configuration for JSON Viewer.
 *
 * ReactJS library is shared among DevTools. Both, the minified (production)
 * version and developer versions of the library are available.
 *
 * In order to use the developer version you need to specify the following
 * in your .mozconfig (see also bug 1181646):
 * ac_add_options --enable-debug-js-modules
 *
 * The path mapping uses paths fallback (a feature supported by RequireJS)
 * See also: http://requirejs.org/docs/api.html#pathsfallbacks
 */
require.config({
  baseUrl: ".",
  paths: {
    "react": [
      "resource://devtools/client/shared/vendor/react-dev",
      "resource://devtools/client/shared/vendor/react"
    ]
  }
});

// Load the main panel module
requirejs(["json-viewer"]);
