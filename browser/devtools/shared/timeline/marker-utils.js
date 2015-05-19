/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file contains utilities for creating elements for markers to be displayed,
 * and parsing out the blueprint to generate correct values for markers.
 */

loader.lazyRequireGetter(this, "L10N",
  "devtools/shared/timeline/global", true);
loader.lazyRequireGetter(this, "TIMELINE_BLUEPRINT",
  "devtools/shared/timeline/global", true);

/**
 * Returns the correct label to display for passed in marker, based
 * off of the blueprints.
 *
 * @param {ProfileTimelineMarker} marker
 * @return {string}
 */
function getMarkerLabel (marker) {
  let blueprint = TIMELINE_BLUEPRINT[marker.name];
  // Either use the label function in the blueprint, or use it directly
  // as a string.
  return typeof blueprint.label === "function" ? blueprint.label(marker) : blueprint.label;
}
exports.getMarkerLabel = getMarkerLabel;

/**
 * Returns the correct generic name for a marker class, like "Function Call"
 * being the general class for JS markers, rather than "setTimeout", etc.
 *
 * @param {string} type
 * @return {string}
 */
function getMarkerClassName (type) {
  let blueprint = TIMELINE_BLUEPRINT[type];
  // Either use the label function in the blueprint, or use it directly
  // as a string.
  let className = typeof blueprint.label === "function" ? blueprint.label() : blueprint.label;

  // If no class name found, attempt to throw a descriptive error how the marker
  // implementor can fix this.
  if (!className) {
    let message = `Could not find marker class name for "${type}".`;
    if (typeof blueprint.label === "function") {
      message += ` The following function must return a class name string when no marker passed: ${blueprint.label}`;
    } else {
      message += ` ${type}.label must be defined in the marker blueprint.`;
    }
    throw new Error(message);
  }

  return className;
}
exports.getMarkerClassName = getMarkerClassName;

/**
 * Returns an array of objects with key/value pairs of what should be rendered
 * in the marker details view.
 *
 * @param {ProfileTimelineMarker} marker
 * @return {Array<object>}
 */
function getMarkerFields (marker) {
  let blueprint = TIMELINE_BLUEPRINT[marker.name];
  return (blueprint.fields || []).reduce((fields, field) => {
    // Ensure this marker has this field present
    if (field.property in marker) {
      let label = field.label;
      let value = marker[field.property];
      // If a formatter function defined, use it to get the
      // value we actually want to display.
      if (typeof field.formatter === "function") {
        value = field.formatter(marker);
      }
      fields.push({ label, value });
    }
    return fields;
  }, []);
}
exports.getMarkerFields = getMarkerFields;

/**
 * Utilites for creating elements for markers.
 */
const DOM = exports.DOM = {
  /**
   * Builds all the fields possible for the given marker. Returns an
   * array of elements to be appended to a parent element.
   *
   * @param {Document} doc
   * @param {ProfileTimelineMarker} marker
   * @return {Array<Element>}
   */
  buildFields: function (doc, marker) {
    let blueprint = TIMELINE_BLUEPRINT[marker.name];
    let fields = getMarkerFields(marker);

    return fields.map(({ label, value }) => DOM.buildNameValueLabel(doc, label, value));
  },

  /**
   * Builds the label representing marker's type.
   *
   * @param {Document} doc
   * @param {ProfileTimelineMarker}
   * @return {Element}
   */
  buildTitle: function (doc, marker) {
    let blueprint = TIMELINE_BLUEPRINT[marker.name];

    let hbox = doc.createElement("hbox");
    hbox.setAttribute("align", "center");

    let bullet = doc.createElement("hbox");
    bullet.className = `marker-details-bullet ${blueprint.colorName}`;

    let title = getMarkerLabel(marker);
    let label = doc.createElement("label");
    label.className = "marker-details-type";
    label.setAttribute("value", title);

    hbox.appendChild(bullet);
    hbox.appendChild(label);

    return hbox;
  },

  /**
   * Builds the duration element, like "Duration: 200ms".
   *
   * @param {Document} doc
   * @param {ProfileTimelineMarker} marker
   * @return {Element}
   */
  buildDuration: function (doc, marker) {
    let label = L10N.getStr("timeline.markerDetail.duration");
    let start = L10N.getFormatStrWithNumbers("timeline.tick", marker.start);
    let end = L10N.getFormatStrWithNumbers("timeline.tick", marker.end);
    let duration = L10N.getFormatStrWithNumbers("timeline.tick", marker.end - marker.start);
    let el = DOM.buildNameValueLabel(doc, label, duration);
    el.classList.add("marker-details-duration");
    el.setAttribute("tooltiptext", `${start} → ${end}`);
    return el;
  },

  /**
   * Builds labels for name:value pairs. Like "Start: 100ms",
   * "Duration: 200ms", ...
   *
   * @param {Document} doc
   * @param string field
   *        String identifier for label's name.
   * @param string value
   *        Label's value.
   * @return {Element}
   */
  buildNameValueLabel: function (doc, field, value) {
    let hbox = doc.createElement("hbox");
    let labelName = doc.createElement("label");
    let labelValue = doc.createElement("label");
    labelName.className = "plain marker-details-labelname";
    labelValue.className = "plain marker-details-labelvalue";
    labelName.setAttribute("value", field);
    labelValue.setAttribute("value", value);
    hbox.appendChild(labelName);
    hbox.appendChild(labelValue);
    return hbox;
  },
};
