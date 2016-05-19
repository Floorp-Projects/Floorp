/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { TIMELINE_BLUEPRINT } = require("devtools/client/performance/modules/markers");

/**
 * This file contains utilities for parsing out the markers blueprint
 * to generate strings to be displayed in the UI.
 */

exports.MarkerBlueprintUtils = {
  /**
   * Takes a marker and a list of marker names that should be hidden, and
   * determines if this marker should be filtered or not.
   *
   * @param object marker
   * @return boolean
   */
  shouldDisplayMarker: function (marker, hiddenMarkerNames) {
    if (!hiddenMarkerNames || hiddenMarkerNames.length == 0) {
      return true;
    }

    // If this marker isn't yet defined in the blueprint, simply check if the
    // entire category of "UNKNOWN" markers are supposed to be visible or not.
    let isUnknown = !(marker.name in TIMELINE_BLUEPRINT);
    if (isUnknown) {
      return hiddenMarkerNames.indexOf("UNKNOWN") == -1;
    }

    return hiddenMarkerNames.indexOf(marker.name) == -1;
  },

  /**
   * Takes a marker and returns the blueprint definition for that marker type,
   * falling back to the UNKNOWN blueprint definition if undefined.
   *
   * @param object marker
   * @return object
   */
  getBlueprintFor: function (marker) {
    return TIMELINE_BLUEPRINT[marker.name] || TIMELINE_BLUEPRINT.UNKNOWN;
  },

  /**
   * Returns the label to display for a marker, based off the blueprints.
   *
   * @param object marker
   * @return string
   */
  getMarkerLabel: function (marker) {
    let blueprint = this.getBlueprintFor(marker);
    let dynamic = typeof blueprint.label === "function";
    let label = dynamic ? blueprint.label(marker) : blueprint.label;
    return label;
  },

  /**
   * Returns the generic label to display for a marker name.
   * (e.g. "Function Call" for JS markers, rather than "setTimeout", etc.)
   *
   * @param string type
   * @return string
   */
  getMarkerGenericName: function (markerName) {
    let blueprint = this.getBlueprintFor({ name: markerName });
    let dynamic = typeof blueprint.label === "function";
    let generic = dynamic ? blueprint.label() : blueprint.label;

    // If no class name found, attempt to throw a descriptive error as to
    // how the marker implementor can fix this.
    if (!generic) {
      let message = `Could not find marker generic name for "${markerName}".`;
      if (typeof blueprint.label === "function") {
        message += ` The following function must return a generic name string when no marker passed: ${blueprint.label}`;
      } else {
        message += ` ${markerName}.label must be defined in the marker blueprint.`;
      }
      throw new Error(message);
    }

    return generic;
  },

  /**
   * Returns an array of objects with key/value pairs of what should be rendered
   * in the marker details view.
   *
   * @param object marker
   * @return array<object>
   */
  getMarkerFields: function (marker) {
    let blueprint = this.getBlueprintFor(marker);
    let dynamic = typeof blueprint.fields === "function";
    let fields = dynamic ? blueprint.fields(marker) : blueprint.fields;

    return Object.entries(fields || {})
      .filter(([_, value]) => dynamic ? true : value in marker)
      .map(([label, value]) => ({ label, value: dynamic ? value : marker[value] }));
  },
};
