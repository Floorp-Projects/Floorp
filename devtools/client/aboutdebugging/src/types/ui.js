/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  ADB_ADDON_STATES,
} = require("devtools/client/shared/remote-debugging/adb/adb-addon");
const {
  DEBUG_TARGET_PANE,
  PAGE_TYPES,
} = require("devtools/client/aboutdebugging/src/constants");

function makeCollapsibilitiesType(isRequired) {
  return (props, propName, componentName, _, propFullName) => {
    if (isRequired && props[propName] === null) {
      return new Error(
        `Missing prop ${propFullName} marked as required in ${componentName}`
      );
    }

    const error = new Error(
      `Invalid prop ${propFullName} (${props[propName]}) supplied to ` +
        `${componentName}. Collapsibilities needs to be a Map<DEBUG_TARGET_PANE, bool>`
    );

    const map = props[propName];

    // check that the prop is a Map
    if (!(map instanceof Map)) {
      return error;
    }

    // check that the keys refer to debug target panes
    const areKeysValid = [...map.keys()].every(x =>
      Object.values(DEBUG_TARGET_PANE).includes(x)
    );
    // check that the values are boolean
    const areValuesValid = [...map.values()].every(x => typeof x === "boolean");
    // error if values or keys fail their checks
    if (!areKeysValid || !areValuesValid) {
      return error;
    }

    return null;
  };
}

function makeLocationType(isRequired) {
  return (props, propName, componentName, _, propFullName) => {
    if (isRequired && props[propName] === null) {
      return new Error(
        `Missing prop ${propFullName} marked as required in ${componentName}`
      );
    }

    // check that location is a string with a semicolon in it
    if (!/\:/.test(props[propName])) {
      return new Error(
        `Invalid prop ${propFullName} (${props[propName]}) supplied to ` +
          `${componentName}. Location needs to be a string with a host:port format`
      );
    }

    return null;
  };
}

const collapsibilities = makeCollapsibilitiesType(false);
collapsibilities.isRequired = makeCollapsibilitiesType(true);

const location = makeLocationType(false);
location.isRequired = makeLocationType(true);

module.exports = {
  adbAddonStatus: PropTypes.oneOf(Object.values(ADB_ADDON_STATES)),
  // a Map<DEBUG_TARGET_PANE, bool>, to flag collapsed/expanded status of the
  // debug target panes
  collapsibilities,
  // a string with "host:port" format, used for network locations
  location,
  page: PropTypes.oneOf(Object.values(PAGE_TYPES)),
};
