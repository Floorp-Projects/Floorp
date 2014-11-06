/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var BaseAssert = require("sdk/test/assert").Assert;

const getOwnIdentifiers = x => [...Object.getOwnPropertyNames(x),
                                ...Object.getOwnPropertySymbols(x)];

/**
 * Whether or not given property descriptors are equivalent. They are
 * equivalent either if both are marked as "conflict" or "required" property
 * or if all the properties of descriptors are equal.
 * @param {Object} actual
 * @param {Object} expected
 */
function equivalentDescriptors(actual, expected) {
  return (actual.conflict && expected.conflict) ||
         (actual.required && expected.required) ||
         equalDescriptors(actual, expected);
}

function equalDescriptors(actual, expected) {
  return actual.get === expected.get &&
         actual.set === expected.set &&
         actual.value === expected.value &&
         !!actual.enumerable === !!expected.enumerable &&
         !!actual.configurable === !!expected.configurable &&
         !!actual.writable === !!expected.writable;
}

/**
 * Whether or not given `target` array contains all the element
 * from a given `source` array.
 */
function containsSet(source, target) {
  return source.some(function(element) {
    return 0 > target.indexOf(element);
  });
}

/**
 * Whether or not given two arrays contain all elements from another.
 */
function equivalentSets(source, target) {
  return containsSet(source, target) && containsSet(target, source);
}

/**
 * Finds name of the property from `source` property descriptor map, that
 * is not equivalent of the name named property in the `target` property
 * descriptor map. If not found `null` is returned instead.
 */
function findNonEquivalentPropertyName(source, target) {
  var value = null;
  getOwnIdentifiers(source).some(function(key) {
    var areEquivalent = false;
    if (!equivalentDescriptors(source[key], target[key])) {
      value = key;
      areEquivalent = true;
    }
    return areEquivalent;
  });
  return value;
}

var AssertDescriptor = {
  equalTraits: {
    value: function equivalentTraits(actual, expected, message) {
      var difference;
      var actualKeys = getOwnIdentifiers(actual);
      var expectedKeys = getOwnIdentifiers(expected);

      if (equivalentSets(actualKeys, expectedKeys)) {
        this.fail({
          operator: "equalTraits",
          message: "Traits define different properties",
          actual: actualKeys.sort().join(","),
          expected: expectedKeys.sort().join(","),
        });
      }
      else if ((difference = findNonEquivalentPropertyName(actual, expected))) {
        this.fail({
          operator: "equalTraits",
          message: "Traits define non-equivalent property `" + difference + "`",
          actual: actual[difference],
          expected: expected[difference]
        });
      }
      else {
        this.pass(message || "Traits are equivalent.");
      }
    }
  }
};

exports.Assert = function Assert() {
  return Object.create(BaseAssert.apply(null, arguments), AssertDescriptor);
};
