/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* global requirejs */

"use strict";

// Send readyState change notification event to the window. It's useful for tests.
JSONView.readyState = "loading";
window.dispatchEvent(new CustomEvent("AppReadyStateChange"));

// Services is required in the Reps bundle but can't be loaded in the json-viewer.
// Since it's only used for the ObjectInspector, that the json-viewer does not use, we
// can mock it. The mock should be removed when we un-bundle reps, (i.e. land individual
// files instead of a big bundle).
define("ServicesMock", () => ({ appinfo: {} }));

/**
 * RequireJS configuration for JSON Viewer.
 *
 * ReactJS library is shared among DevTools. The minified (production) version
 * of the library is always available, and is used by default.
 *
 * In order to use the developer version you need to specify the following
 * in your .mozconfig (see also bug 1181646):
 * ac_add_options --enable-debug-js-modules
 *
 * React module ID is using exactly the same (relative) path as the rest
 * of the code base, so it's consistent and modules can be easily reused.
 */
require.config({
  baseUrl: "resource://devtools-client-jsonview/",
  paths: {
    "devtools/client/jsonview": "resource://devtools-client-jsonview",
    "devtools/client/shared": "resource://devtools-client-shared",
    "devtools/shared": "resource://devtools/shared",
    "devtools/client/shared/vendor/react": JSONView.debugJsModules
      ? "resource://devtools-client-shared/vendor/react-dev"
      : "resource://devtools-client-shared/vendor/react",
    "devtools/client/shared/vendor/react-dom": JSONView.debugJsModules
      ? "resource://devtools-client-shared/vendor/react-dom-dev"
      : "resource://devtools-client-shared/vendor/react-dom",
    "devtools/client/shared/vendor/react-prop-types": JSONView.debugJsModules
      ? "resource://devtools-client-shared/vendor/react-prop-types-dev"
      : "resource://devtools-client-shared/vendor/react-prop-types",
    "devtools/client/shared/vendor/react-dom-test-utils": JSONView.debugJsModules
      ? "resource://devtools-client-shared/vendor/react-dom-test-utils-dev"
      : "resource://devtools-client-shared/vendor/react-dom-test-utils",
    Services: "resource://devtools-client-shared/vendor/react-prop-types",
  },
  map: {
    "*": {
      Services: "ServicesMock",
    },
  },
});

// Load the main panel module
requirejs(["json-viewer"]);
