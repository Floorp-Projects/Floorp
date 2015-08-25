/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { L10N } = require("devtools/performance/global");
const { Formatters } = require("devtools/performance/marker-utils");

/**
 * A simple schema for mapping markers to the timeline UI. The keys correspond
 * to marker names, while the values are objects with the following format:
 *
 * - group: The row index in the overview graph; multiple markers
 *          can be added on the same row. @see <overview.js/buildGraphImage>
 * - label: The label used in the waterfall to identify the marker. Can be a
 *          string or just a function that accepts the marker and returns a
 *          string, if you want to use a dynamic property for the main label.
 *          If you use a function for a label, it *must* handle the case where
 *          no marker is provided for a main label to describe all markers of
 *          this type.
 * - colorName: The label of the DevTools color used for this marker. If
 *              adding a new color, be sure to check that there's an entry
 *              for `.marker-details-bullet.{COLORNAME}` for the equivilent
 *              entry in ./browser/themes/shared/devtools/performance.inc.css
 *              https://developer.mozilla.org/en-US/docs/Tools/DevToolsColors
 * - collapsible: Whether or not this marker can contain other markers it
 *                eclipses, and becomes collapsible to reveal its nestable children.
 *                Defaults to true.
 * - nestable: Whether or not this marker can be nested inside an eclipsing
 *             collapsible marker. Defaults to true.
 * - fields: An optional array of marker properties you wish to display in the
 *           marker details view. For example, a field in the array such as
 *           { property: "aCauseName", label: "Cause" } would render a string
 *           like `Cause: ${marker.aCauseName}` in the marker details view.
 *           Each `field` item may take the following properties:
 *           - property: The property that must exist on the marker to render,
 *                       and the value of the property will be displayed.
 *           - label: The name of the property that should be displayed.
 *           - formatter: If a formatter is provided, instead of directly using
 *                        the `property` property on the marker, the marker is
 *                        passed into the formatter function to determine the
 *                        displayed value.
 *            Can also be a function that returns an object. Each key in the object
 *            will be rendered as a field, with its value rendering as the value.
 *
 * Whenever this is changed, browser_timeline_waterfall-styles.js *must* be
 * updated as well.
 */
const TIMELINE_BLUEPRINT = {
  /* Default definition used for markers that occur but
   * are not defined here. Should ultimately be defined, but this gives
   * us room to work on the front end separately from the platform. */
  "UNKNOWN": {
    group: 2,
    colorName: "graphs-grey",
    label: Formatters.UnknownLabel,
  },

  /* Group 0 - Reflow and Rendering pipeline */
  "Styles": {
    group: 0,
    colorName: "graphs-purple",
    label: L10N.getStr("marker.label.styles"),
    fields: Formatters.StylesFields,
  },
  "Reflow": {
    group: 0,
    colorName: "graphs-purple",
    label: L10N.getStr("marker.label.reflow"),
  },
  "Paint": {
    group: 0,
    colorName: "graphs-green",
    label: L10N.getStr("marker.label.paint"),
  },
  "Composite": {
    group: 0,
    colorName: "graphs-green",
    label: L10N.getStr("marker.label.composite"),
  },

  /* Group 1 - JS */
  "DOMEvent": {
    group: 1,
    colorName: "graphs-yellow",
    label: L10N.getStr("marker.label.domevent"),
    fields: Formatters.DOMEventFields,
  },
  "Javascript": {
    group: 1,
    colorName: "graphs-yellow",
    label: Formatters.JSLabel,
    fields: Formatters.JSFields
  },
  "Parse HTML": {
    group: 1,
    colorName: "graphs-yellow",
    label: L10N.getStr("marker.label.parseHTML"),
  },
  "Parse XML": {
    group: 1,
    colorName: "graphs-yellow",
    label: L10N.getStr("marker.label.parseXML"),
  },
  "GarbageCollection": {
    group: 1,
    colorName: "graphs-red",
    label: Formatters.GCLabel,
    fields: Formatters.GCFields,
  },
  "nsCycleCollector::Collect": {
    group: 1,
    colorName: "graphs-red",
    label: L10N.getStr("marker.label.cycleCollection"),
    fields: Formatters.CycleCollectionFields,
  },
  "nsCycleCollector::ForgetSkippable": {
    group: 1,
    colorName: "graphs-red",
    label: L10N.getStr("marker.label.cycleCollection.forgetSkippable"),
    fields: Formatters.CycleCollectionFields,
  },

  /* Group 2 - User Controlled */
  "ConsoleTime": {
    group: 2,
    colorName: "graphs-blue",
    label: sublabelForProperty(L10N.getStr("marker.label.consoleTime"), "causeName"),
    fields: [{
      property: "causeName",
      label: L10N.getStr("marker.field.consoleTimerName")
    }],
    nestable: false,
    collapsible: false,
  },
  "TimeStamp": {
    group: 2,
    colorName: "graphs-blue",
    label: sublabelForProperty(L10N.getStr("marker.label.timestamp"), "causeName"),
    fields: [{
      property: "causeName",
      label: "Label:"
    }],
    collapsible: false,
  },
};

/**
 * Takes a main label (like "Timestamp") and a property,
 * and returns a marker that will print out the property
 * value for a marker if it exists ("Timestamp (rendering)"),
 * or just the main label if it does not.
 */
function sublabelForProperty (mainLabel, prop) {
  return (marker={}) => marker[prop] ? `${mainLabel} (${marker[prop]})` : mainLabel;
}

// Exported symbols.
exports.TIMELINE_BLUEPRINT = TIMELINE_BLUEPRINT;
