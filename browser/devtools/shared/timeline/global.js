/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");

Cu.import("resource:///modules/devtools/ViewHelpers.jsm");

/**
 * Localization convenience methods.
 */
const STRINGS_URI = "chrome://browser/locale/devtools/timeline.properties";
const L10N = new ViewHelpers.L10N(STRINGS_URI);

/**
 * A simple schema for mapping markers to the timeline UI. The keys correspond
 * to marker names, while the values are objects with the following format:
 *
 *   - group: the row index in the timeline overview graph; multiple markers
 *            can be added on the same row. @see <overview.js/buildGraphImage>
 *   - label: the label used in the waterfall to identify the marker. Can be a
 *            string or just a function that accepts the marker and returns a string,
 *            if you want to use a dynamic property for the main label.
 *   - colorName: the label of the DevTools color used for this marker. If adding
 *                a new color, be sure to check that there's an entry for
 *                `.marker-details-bullet.{COLORNAME}` for the equivilent entry
 *                in ./browser/themes/shared/devtools/performance.inc.css
 *                https://developer.mozilla.org/en-US/docs/Tools/DevToolsColors
 *   - fields: An optional array of marker properties you wish to display in the
 *             marker details view. For example, a field of
 *             { property: "aCauseName", label: "Cause" }
 *             would render a field like `Cause: ${marker.aCauseName}`.
 *             Each `field` item may take the following properties:
 *
 *             - property: The property that must exist on the marker to render, and
 *                         the value of the property will be displayed.
 *             - label: The name of the property that should be displayed.
 *             - formatter: If a formatter is provided, instead of directly using the `property`
 *                          property on the marker, the marker is passed into the formatter
 *                          function to determine the display value.
 *
 * Whenever this is changed, browser_timeline_waterfall-styles.js *must* be
 * updated as well.
 */
const TIMELINE_BLUEPRINT = {
  /* Group 0 - Reflow and Rendering pipeline */
  "Styles": {
    group: 0,
    colorName: "highlight-pink",
    label: L10N.getStr("timeline.label.styles2")
  },
  "Reflow": {
    group: 0,
    colorName: "highlight-pink",
    label: L10N.getStr("timeline.label.reflow2")
  },
  "Paint": {
    group: 0,
    colorName: "highlight-green",
    label: L10N.getStr("timeline.label.paint")
  },

  /* Group 1 - JS */
  "DOMEvent": {
    group: 1,
    colorName: "highlight-lightorange",
    label: L10N.getStr("timeline.label.domevent"),
    fields: [{
      property: "type",
      label: L10N.getStr("timeline.markerDetail.DOMEventType")
    }, {
      property: "eventPhase",
      label: L10N.getStr("timeline.markerDetail.DOMEventPhase"),
      formatter: getEventPhaseName
    }]
  },
  "Javascript": {
    group: 1,
    colorName: "highlight-lightorange",
    label: (marker) => marker.causeName,
  },
  "Parse HTML": {
    group: 1,
    colorName: "highlight-lightorange",
    label: L10N.getStr("timeline.label.parseHTML")
  },
  "Parse XML": {
    group: 1,
    colorName: "highlight-lightorange",
    label: L10N.getStr("timeline.label.parseXML")
  },
  "GarbageCollection": {
    group: 1,
    colorName: "highlight-red",
    label: getGCLabel,
    fields: [
      { property: "causeName", label: "Reason:" },
      { property: "nonincrementalReason", label: "Non-incremental Reason:" }
    ]
  },

  /* Group 2 - User Controlled */
  "ConsoleTime": {
    group: 2,
    colorName: "highlight-bluegrey",
    label: L10N.getStr("timeline.label.consoleTime"),
    fields: [{
      property: "causeName",
      label: L10N.getStr("timeline.markerDetail.consoleTimerName")
    }]
  },
  "TimeStamp": {
    group: 2,
    colorName: "highlight-purple",
    label: L10N.getStr("timeline.label.timestamp")
  },
};

/**
 * A series of formatters used by the blueprint.
 */

function getEventPhaseName (marker) {
  if (marker.eventPhase === Ci.nsIDOMEvent.AT_TARGET) {
    return L10N.getStr("timeline.markerDetail.DOMEventTargetPhase");
  } else if (marker.eventPhase === Ci.nsIDOMEvent.CAPTURING_PHASE) {
    return L10N.getStr("timeline.markerDetail.DOMEventCapturingPhase");
  } else if (marker.eventPhase === Ci.nsIDOMEvent.BUBBLING_PHASE) {
    return L10N.getStr("timeline.markerDetail.DOMEventBubblingPhase");
  }
}

function getGCLabel (marker) {
  let label = L10N.getStr("timeline.label.garbageCollection");
  // Only if a `nonincrementalReason` exists, do we want to label
  // this as a non incremental GC event.
  if ("nonincrementalReason" in marker) {
    label = `${label} (Non-incremental)`;
  }
  return label;
}

// Exported symbols.
exports.L10N = L10N;
exports.TIMELINE_BLUEPRINT = TIMELINE_BLUEPRINT;
