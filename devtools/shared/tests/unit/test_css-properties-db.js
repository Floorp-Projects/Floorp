/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that css-properties-db matches platform.

"use strict";

const DOMUtils = Components.classes["@mozilla.org/inspector/dom-utils;1"]
                           .getService(Components.interfaces.inIDOMUtils);

const {PSEUDO_ELEMENTS} = require("devtools/shared/css-properties-db");

function run_test() {
  // Check that the platform and client match for pseudo elements.
  let foundPseudoElements = 0;
  const platformPseudoElements = DOMUtils.getCSSPseudoElementNames();
  const instructions = "If this assertion fails then it means that pseudo elements " +
                       "have been added, removed, or changed on the platform and need " +
                       "to be updated on the static client-side list of pseudo " +
                       "elements within the devtools. " +
                       "See devtools/shared/css-properties-db.js and " +
                       "exports.PSEUDO_ELEMENTS for information on how to update the " +
                       "list of pseudo elements to fix this test.";

  for (let element of PSEUDO_ELEMENTS) {
    const hasElement = platformPseudoElements.includes(element);
    ok(hasElement,
       `"${element}" pseudo element from the client-side CSS properties database was ` +
       `found on the platform. ${instructions}`);
    foundPseudoElements += hasElement ? 1 : 0;
  }

  equal(foundPseudoElements, platformPseudoElements.length,
        `The client side CSS properties database of psuedo element names should match ` +
        `those found on the platform. ${instructions}`);
}
