/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that the devtool's client-side CSS properties database is in sync with the values
 * on the platform (in Nightly only). If they are not, then `mach devtools-css-db` needs
 * to be run to make everything up to date. Nightly, aurora, beta, and release may have
 * different CSS properties and values. These are based on preferences and compiler flags.
 *
 * This test broke uplifts as the database needed to be regenerated every uplift. The
 * combination of compiler flags and preferences means that it's too difficult to
 * statically determine which properties are enabled between Firefox releases.
 *
 * Because of these difficulties, the database only needs to be up to date with Nightly.
 * It is a fallback that is only used if the remote debugging protocol doesn't support
 * providing a CSS database, so it's ok if the provided properties don't exactly match
 * the inspected target in this particular case.
 */

"use strict";

const {PSEUDO_ELEMENTS, CSS_PROPERTIES, PREFERENCES} = require("devtools/shared/css/generated/properties-db");
const {generateCssProperties} = require("devtools/server/actors/css-properties");
const {Preferences} = require("resource://gre/modules/Preferences.jsm");
const InspectorUtils = require("InspectorUtils");

function run_test() {
  const propertiesErrorMessage = "If this assertion fails, then the client side CSS " +
                                 "properties list in devtools is out of sync with the " +
                                 "CSS properties on the platform. To fix this " +
                                 "assertion run `mach devtools-css-db` to re-generate " +
                                 "the client side properties.";

  // Check that the platform and client match for pseudo elements.
  deepEqual(PSEUDO_ELEMENTS, InspectorUtils.getCSSPseudoElementNames(),
            "The pseudo elements match on the client and platform. " +
            propertiesErrorMessage);

  /**
   * Check that the platform and client match for the details on their CSS properties.
   * Enumerate each property to aid in debugging. Sometimes these properties don't
   * completely agree due to differences in preferences. Check the currently set
   * preference for that property to see if it's enabled.
   */
  const platformProperties = generateCssProperties();

  for (const propertyName in CSS_PROPERTIES) {
    const platformProperty = platformProperties[propertyName];
    const clientProperty = CSS_PROPERTIES[propertyName];
    const deepEqual = isJsonDeepEqual(platformProperty, clientProperty);

    // The "all" property can contain information that can be turned on and off by
    // preferences. These values can be different between OSes, so ignore the equality
    // check for this property, since this is likely to fail.
    if (propertyName === "all") {
      continue;
    }

    if (deepEqual) {
      ok(true, `The static database and platform match for "${propertyName}".`);
    } else {
      ok(false, `The static database and platform do not match for ` + `
        "${propertyName}". ${propertiesErrorMessage}`);
    }
  }

  /**
   * Check that the list of properties on the platform and client are the same. If
   * they are not, check that there may be preferences that are disabling them on the
   * target platform.
   */
  const mismatches = getKeyMismatches(platformProperties, CSS_PROPERTIES)
    // Filter out OS-specific properties.
    .filter(name => name && !name.includes("-moz-osx-"));

  if (mismatches.length === 0) {
    ok(true, "No client and platform CSS property database mismatches were found.");
  }

  mismatches.forEach(propertyName => {
    if (getPreference(propertyName) === false) {
      ok(true, `The static database and platform do not agree on the property ` +
               `"${propertyName}" This is ok because it is currently disabled through ` +
               `a preference.`);
    } else {
      ok(false, `The static database and platform do not agree on the property ` +
                `"${propertyName}" ${propertiesErrorMessage}`);
    }
  });
}

/**
 * Check JSON-serializable objects for deep equality.
 */
function isJsonDeepEqual(a, b) {
  // Handle primitives.
  if (a === b) {
    return true;
  }

  // Handle arrays.
  if (Array.isArray(a) && Array.isArray(b)) {
    if (a.length !== b.length) {
      return false;
    }
    for (let i = 0; i < a.length; i++) {
      if (!isJsonDeepEqual(a[i], b[i])) {
        return false;
      }
    }
    return true;
  }

  // Handle objects
  if (typeof a === "object" && typeof b === "object") {
    for (const key in a) {
      if (!isJsonDeepEqual(a[key], b[key])) {
        return false;
      }
    }

    return Object.keys(a).length === Object.keys(b).length;
  }

  // Not something handled by these cases, therefore not equal.
  return false;
}

/**
 * Take the keys of two objects, and return the ones that don't match.
 *
 * @param {Object} a
 * @param {Object} b
 * @return {Array} keys
 */
function getKeyMismatches(a, b) {
  const aNames = Object.keys(a);
  const bNames = Object.keys(b);
  const aMismatches = aNames.filter(key => !bNames.includes(key));
  const bMismatches = bNames.filter(key => {
    return !aNames.includes(key) && !aMismatches.includes(key);
  });

  return aMismatches.concat(bMismatches);
}

/**
 * Get the preference value of whether this property is enabled. Returns an empty string
 * if no preference exists.
 *
 * @param {String} propertyName
 * @return {Boolean|undefined}
 */
function getPreference(propertyName) {
  const preference = PREFERENCES.find(([prefPropertyName, preferenceKey]) => {
    return prefPropertyName === propertyName && !!preferenceKey;
  });

  if (preference) {
    return Preferences.get(preference[1]);
  }
  return undefined;
}
