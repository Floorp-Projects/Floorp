/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const constants = require("devtools/client/dom/content/constants");

/**
 * Initial state definition
 */
function getInitialState() {
  return new Map();
}

/**
 * Maintain a cache of received grip responses from the backend.
 */
function grips(state = getInitialState(), action) {
  // This reducer supports only one action, fetching actor properties
  // from the backend so, bail out if we are dealing with any other
  // action.
  if (action.type != constants.FETCH_PROPERTIES) {
    return state;
  }

  switch (action.status) {
    case "start":
      return onRequestProperties(state, action);
    case "done":
      return onReceiveProperties(state, action);
  }

  return state;
}

/**
 * Handle requestProperties action
 */
function onRequestProperties(state, action) {
  return state;
}

/**
 * Handle receiveProperties action
 */
function onReceiveProperties(cache, action) {
  const response = action.response;
  const from = response.from;
  const className = action.grip?.getGrip
    ? action.grip.getGrip().class
    : action.grip.class;

  // Properly deal with getters.
  mergeProperties(response);

  // Compute list of requested children.
  const previewProps = response.preview ? response.preview.ownProperties : null;
  const ownProps = response.ownProperties || previewProps || [];

  const props = Object.keys(ownProps).map(key => {
    // Array indexes as a special case. We convert any keys that are string
    // representations of integers to integers.
    if (className === "Array" && isInteger(key)) {
      key = parseInt(key, 10);
    }
    return new Property(key, ownProps[key], key);
  });

  props.sort(sortName);

  // Return new state/map.
  const newCache = new Map(cache);
  newCache.set(from, props);

  return newCache;
}

// Helpers

function mergeProperties(response) {
  const { ownProperties } = response;

  // 'safeGetterValues' is new and isn't necessary defined on old grips.
  const safeGetterValues = response.safeGetterValues || {};

  // Merge the safe getter values into one object such that we can use it
  // in variablesView.
  for (const name of Object.keys(safeGetterValues)) {
    if (name in ownProperties) {
      const { getterValue, getterPrototypeLevel } = safeGetterValues[name];
      ownProperties[name].getterValue = getterValue;
      ownProperties[name].getterPrototypeLevel = getterPrototypeLevel;
    } else {
      ownProperties[name] = safeGetterValues[name];
    }
  }
}

function sortName(a, b) {
  // Display non-enumerable properties at the end.
  if (!a.value.enumerable && b.value.enumerable) {
    return 1;
  }
  if (a.value.enumerable && !b.value.enumerable) {
    return -1;
  }
  return a.name > b.name ? 1 : -1;
}

function isInteger(n) {
  // We use parseInt(n, 10) == n to disregard scientific notation e.g. "3e24"
  return isFinite(n) && parseInt(n, 10) == n;
}

function Property(name, value, key) {
  this.name = name;
  this.value = value;
  this.key = key;
}

// Exports from this module
exports.grips = grips;
exports.Property = Property;
