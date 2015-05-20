/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");
loader.lazyRequireGetter(this, "ViewHelpers",
  "resource:///modules/devtools/ViewHelpers.jsm", true);
loader.lazyRequireGetter(this, "Services");

// String used to fill in platform data when it should be hidden.
const GECKO_SYMBOL = "(Gecko)";

/**
 * Localization convenience methods.
 */
const STRINGS_URI = "chrome://browser/locale/devtools/timeline.properties";
const L10N = new ViewHelpers.L10N(STRINGS_URI);

/**
 * Monitor "show-platform-data" pref.
 */
const prefs = new ViewHelpers.Prefs("devtools.performance.ui", {
  showPlatformData: ["Bool", "show-platform-data"]
});

let SHOW_PLATFORM_DATA = Services.prefs.getBoolPref("devtools.performance.ui.show-platform-data");
prefs.registerObserver();
prefs.on("pref-changed", (_,  prefName, prefValue) => {
  if (prefName === "showPlatformData") {
    SHOW_PLATFORM_DATA = prefValue;
  }
});

/**
 * A simple schema for mapping markers to the timeline UI. The keys correspond
 * to marker names, while the values are objects with the following format:
 *
 *   - group: the row index in the timeline overview graph; multiple markers
 *            can be added on the same row. @see <overview.js/buildGraphImage>
 *   - label: the label used in the waterfall to identify the marker. Can be a
 *            string or just a function that accepts the marker and returns a string,
 *            if you want to use a dynamic property for the main label.
 *            If you use a function for a label, it *must* handle the case where
 *            no marker is provided for a main label to describe all markers of this type.
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
 *
 *             Can also be a function that returns an object. Each key in the object
 *             will be rendered as a field, with its value rendering as the value.
 *
 * Whenever this is changed, browser_timeline_waterfall-styles.js *must* be
 * updated as well.
 */
const TIMELINE_BLUEPRINT = {
  /* Group 0 - Reflow and Rendering pipeline */
  "Styles": {
    group: 0,
    colorName: "graphs-purple",
    label: L10N.getStr("timeline.label.styles2"),
    fields: getStylesFields,
  },
  "Reflow": {
    group: 0,
    colorName: "graphs-purple",
    label: L10N.getStr("timeline.label.reflow2")
  },
  "Paint": {
    group: 0,
    colorName: "graphs-green",
    label: L10N.getStr("timeline.label.paint")
  },

  /* Group 1 - JS */
  "DOMEvent": {
    group: 1,
    colorName: "graphs-yellow",
    label: L10N.getStr("timeline.label.domevent"),
    fields: getDOMEventFields,
  },
  "Javascript": {
    group: 1,
    colorName: "graphs-yellow",
    label: getJSLabel,
    fields: getJSFields,
  },
  "Parse HTML": {
    group: 1,
    colorName: "graphs-yellow",
    label: L10N.getStr("timeline.label.parseHTML")
  },
  "Parse XML": {
    group: 1,
    colorName: "graphs-yellow",
    label: L10N.getStr("timeline.label.parseXML")
  },
  "GarbageCollection": {
    group: 1,
    colorName: "graphs-red",
    label: getGCLabel,
    fields: [
      { property: "causeName", label: "Reason:" },
      { property: "nonincrementalReason", label: "Non-incremental Reason:" }
    ]
  },

  /* Group 2 - User Controlled */
  "ConsoleTime": {
    group: 2,
    colorName: "graphs-grey",
    label: sublabelForProperty(L10N.getStr("timeline.label.consoleTime"), "causeName"),
    fields: [{
      property: "causeName",
      label: L10N.getStr("timeline.markerDetail.consoleTimerName")
    }]
  },
  "TimeStamp": {
    group: 2,
    colorName: "graphs-blue",
    label: sublabelForProperty("Timestamp", "causeName"),
    fields: [{
      property: "causeName",
      label: "Label:"
    }]
  },
};

/**
 * A series of formatters used by the blueprint.
 */

function getGCLabel (marker={}) {
  let label = L10N.getStr("timeline.label.garbageCollection");
  // Only if a `nonincrementalReason` exists, do we want to label
  // this as a non incremental GC event.
  if ("nonincrementalReason" in marker) {
    label = `${label} (Non-incremental)`;
  }
  return label;
}

/**
 * Mapping of JS marker causes to a friendlier form. Only
 * markers that are considered "from content" should be labeled here.
 */
const JS_MARKER_MAP = {
  "<script> element":          "Script Tag",
  "setInterval handler":       "setInterval",
  "setTimeout handler":        "setTimeout",
  "FrameRequestCallback":      "requestAnimationFrame",
  "promise callback":          "Promise Callback",
  "promise initializer":       "Promise Init",
  "Worker runnable":           "Worker",
  "javascript: URI":           "JavaScript URI",
  // As far as I know, the difference between these two
  // event handler markers are differences in their webidl implementation.
  "EventHandlerNonNull":       "Event Handler",
  "EventListener.handleEvent": "Event Handler",
};

function getJSLabel (marker={}) {
  let generic = L10N.getStr("timeline.label.javascript2");
  if ("causeName" in marker) {
    return JS_MARKER_MAP[marker.causeName] || generic;
  }
  return generic;
}

/**
 * Returns a hash for computing a fields object for a JS marker. If the cause
 * is considered content (so an entry exists in the JS_MARKER_MAP), do not display it
 * since it's redundant with the label. Otherwise for Gecko code, either display
 * the cause, or "(Gecko)", depending on if "show-platform-data" is set.
 */
function getJSFields (marker) {
  if ("causeName" in marker && !JS_MARKER_MAP[marker.causeName]) {
    return { Reason: (SHOW_PLATFORM_DATA ? marker.causeName : GECKO_SYMBOL) };
  }
}

function getDOMEventFields (marker) {
  let fields = Object.create(null);
  if ("type" in marker) {
    fields[L10N.getStr("timeline.markerDetail.DOMEventType")] = marker.type;
  }
  if ("eventPhase" in marker) {
    let phase;
    if (marker.eventPhase === Ci.nsIDOMEvent.AT_TARGET) {
      phase = L10N.getStr("timeline.markerDetail.DOMEventTargetPhase");
    } else if (marker.eventPhase === Ci.nsIDOMEvent.CAPTURING_PHASE) {
      phase = L10N.getStr("timeline.markerDetail.DOMEventCapturingPhase");
    } else if (marker.eventPhase === Ci.nsIDOMEvent.BUBBLING_PHASE) {
      phase = L10N.getStr("timeline.markerDetail.DOMEventBubblingPhase");
    }
    fields[L10N.getStr("timeline.markerDetail.DOMEventPhase")] = phase;
  }
  return fields;
}

function getStylesFields (marker) {
  if ("restyleHint" in marker) {
    return { "Restyle Hint": marker.restyleHint.replace(/eRestyle_/g, "") };
  }
}

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
exports.L10N = L10N;
exports.TIMELINE_BLUEPRINT = TIMELINE_BLUEPRINT;
