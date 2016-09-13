/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the devtool's client-side css-properties-db matches the values on the
// platform.

"use strict";

const DOMUtils = Components.classes["@mozilla.org/inspector/dom-utils;1"]
                           .getService(Components.interfaces.inIDOMUtils);

const {PSEUDO_ELEMENTS, CSS_PROPERTIES} = require("devtools/shared/css-properties-db");
const {generateCssProperties} = require("devtools/server/actors/css-properties");

function run_test() {
  // Check that the platform and client match for pseudo elements.
  deepEqual(PSEUDO_ELEMENTS, DOMUtils.getCSSPseudoElementNames(),
            "If this assertion fails, then the client side CSS pseudo elements list in " +
            "devtools is out of date with the pseudo elements on the platform. To fix " +
            "this assertion open devtools/shared/css-properties-db.js and follow the " +
            "instructions above the CSS_PSEUDO_ELEMENTS on how to re-generate the list.");

  const propertiesErrorMessage = "If this assertion fails, then the client side CSS " +
                                 "properties list in devtools is out of date with the " +
                                 "CSS properties on the platform. To fix this " +
                                 "assertion open devtools/shared/css-properties-db.js " +
                                 "and follow the instructions above the CSS_PROPERTIES " +
                                 "on how to re-generate the list.";

  // Check that the platform and client match for CSS properties. Enumerate each property
  // to aid in debugging.
  const platformProperties = generateCssProperties();
  for (let propertyName in CSS_PROPERTIES) {
    const platformProperty = platformProperties[propertyName];
    const clientProperty = CSS_PROPERTIES[propertyName];
    deepEqual(platformProperty, clientProperty,
      `Client and server match for "${propertyName}". ${propertiesErrorMessage}\n`);
  }

  // Filter out OS-specific properties.
  const platformPropertyNames = Object.keys(platformProperties).filter(
    name => name.indexOf("-moz-osx-") === -1);
  const clientPlatformNames = Object.keys(CSS_PROPERTIES);

  deepEqual(platformPropertyNames, clientPlatformNames,
        `The client side CSS properties database names match those found on the ` +
        `platform. ${propertiesErrorMessage}`);
}
