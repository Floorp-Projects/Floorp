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
// custom-formatter is required in the Reps bundle but 1. we don't need in the JSON Viewer,
// and 2. it causes issues as this requires the ObjectInspector, which can't be loaded
// via requirejs.
define("CustomFormatterMock", () => ({}));

/**
 * RequireJS configuration for JSON Viewer.
 *
 * React module ID is using exactly the same (relative) path as the rest
 * of the code base, so it's consistent and modules can be easily reused.
 */
require.config({
  baseUrl: "resource://devtools/client/jsonview/",
  paths: {
    devtools: "resource://devtools",
  },
  map: {
    "*": {
      Services: "ServicesMock",
      "devtools/client/shared/components/reps/reps/custom-formatter":
        "CustomFormatterMock",
    },
  },
});

// Load the main panel module
requirejs(["json-viewer"]);
