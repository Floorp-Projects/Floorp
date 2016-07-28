/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { L10N } = require("devtools/client/performance/modules/global");
const { Formatters } = require("devtools/client/performance/modules/marker-formatters");

/**
 * A simple schema for mapping markers to the timeline UI. The keys correspond
 * to marker names, while the values are objects with the following format:
 *
 * - group: The row index in the overview graph; multiple markers
 *          can be added on the same row. @see <overview.js/buildGraphImage>
 * - label: The label used in the waterfall to identify the marker. Can be a
 *          string or just a function that accepts the marker and returns a
 *          string (if you want to use a dynamic property for the main label).
 *          If you use a function for a label, it *must* handle the case where
 *          no marker is provided, to get a generic label used to describe
 *          all markers of this type.
 * - fields: The fields used in the marker details view to display more
 *           information about a currently selected marker. Can either be an
 *           object of fields, or simply a function that accepts the marker and
 *           returns such an object (if you want to use properties dynamically).
 *           For example, a field in the object such as { "Cause": "causeName" }
 *           would render something like `Cause: ${marker.causeName}` in the UI.
 * - colorName: The label of the DevTools color used for this marker. If
 *              adding a new color, be sure to check that there's an entry
 *              for `.marker-color-graphs-{COLORNAME}` for the equivilent
 *              entry in "./devtools/client/themes/performance.css"
 *              https://developer.mozilla.org/en-US/docs/Tools/DevToolsColors
 * - collapsible: Whether or not this marker can contain other markers it
 *                eclipses, and becomes collapsible to reveal its nestable
 *                children. Defaults to true.
 * - nestable: Whether or not this marker can be nested inside an eclipsing
 *             collapsible marker. Defaults to true.
 */
const TIMELINE_BLUEPRINT = {
  /* Default definition used for markers that occur but are not defined here.
   * Should ultimately be defined, but this gives us room to work on the
   * front end separately from the platform. */
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
  "CompositeForwardTransaction": {
    group: 0,
    colorName: "graphs-bluegrey",
    label: L10N.getStr("marker.label.compositeForwardTransaction"),
  },

  /* Group 1 - JS */

  "DOMEvent": {
    group: 1,
    colorName: "graphs-yellow",
    label: L10N.getStr("marker.label.domevent"),
    fields: Formatters.DOMEventFields,
  },
  "document::DOMContentLoaded": {
    group: 1,
    colorName: "graphs-full-red",
    label: "DOMContentLoaded"
  },
  "document::Load": {
    group: 1,
    colorName: "graphs-full-blue",
    label: "Load"
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
  "MinorGC": {
    group: 1,
    colorName: "graphs-red",
    label: L10N.getStr("marker.label.minorGC"),
    fields: Formatters.MinorGCFields,
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
  "Worker": {
    group: 1,
    colorName: "graphs-orange",
    label: L10N.getStr("marker.label.worker"),
    fields: Formatters.WorkerFields
  },
  "MessagePort": {
    group: 1,
    colorName: "graphs-orange",
    label: L10N.getStr("marker.label.messagePort"),
    fields: Formatters.MessagePortFields
  },

  /* Group 2 - User Controlled */

  "ConsoleTime": {
    group: 2,
    colorName: "graphs-blue",
    label: Formatters.labelForProperty(L10N.getStr("marker.label.consoleTime"),
                                       "causeName"),
    fields: Formatters.ConsoleTimeFields,
    nestable: false,
    collapsible: false,
  },
  "TimeStamp": {
    group: 2,
    colorName: "graphs-blue",
    label: Formatters.labelForProperty(L10N.getStr("marker.label.timestamp"),
                                       "causeName"),
    fields: Formatters.TimeStampFields,
    collapsible: false,
  },
};

// Exported symbols.
exports.TIMELINE_BLUEPRINT = TIMELINE_BLUEPRINT;
