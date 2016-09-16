/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/*
 * This is an xpcshell script that runs to generate a static list of CSS properties
 * as known by the platform. It is run from ./mach_commands.py by running
 * `mach devtools-css-db`.
 */
var {require} = Components.utils.import("resource://devtools/shared/Loader.jsm", {});
var {generateCssProperties} = require("devtools/server/actors/css-properties");

// Output JSON
dump(JSON.stringify({
  cssProperties: cssProperties(),
  pseudoElements: pseudoElements()
}));

/*
 * A list of CSS Properties and their various characteristics. This is used on the
 * client-side when the CssPropertiesActor is not found, or when the client and server
 * are the same version. A single property takes the form:
 *
 *  "animation": {
 *    "isInherited": false,
 *    "supports": [ 7, 9, 10 ]
 *  }
 */
function cssProperties() {
  const properties = generateCssProperties();
  for (let key in properties) {
    // Ignore OS-specific properties
    if (key.indexOf("-moz-osx-") !== -1) {
      properties[key] = undefined;
    }
  }
  return properties;
}

/**
 * The list of all CSS Pseudo Elements.
 */
function pseudoElements() {
  const {classes: Cc, interfaces: Ci} = Components;
  const domUtils = Cc["@mozilla.org/inspector/dom-utils;1"]
                             .getService(Ci.inIDOMUtils);
  return domUtils.getCSSPseudoElementNames();
}
