/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu, Cr } = require("chrome");
const { ViewHelpers } = require("resource:///modules/devtools/ViewHelpers.jsm");

// String used to fill in platform data when it should be hidden.
const GECKO_SYMBOL = "(Gecko)";

/**
 * Localization convenience methods.
 + TODO: merge these into a single file: Bug 1082695.
 */
const L10N = new ViewHelpers.MultiL10N([
  "chrome://browser/locale/devtools/timeline.properties",
  "chrome://browser/locale/devtools/profiler.properties"
]);

/**
 * A list of preferences for this tool. The values automatically update
 * if somebody edits edits about:config or the prefs change somewhere else.
 */
const PREFS = new ViewHelpers.Prefs("devtools.performance", {
  "show-platform-data": ["Bool", "ui.show-platform-data"],
  "hidden-markers": ["Json", "timeline.hidden-markers"],
  "memory-sample-probability": ["Float", "memory.sample-probability"],
  "memory-max-log-length": ["Int", "memory.max-log-length"],
  "profiler-buffer-size": ["Int", "profiler.buffer-size"],
  "profiler-sample-frequency": ["Int", "profiler.sample-frequency-khz"],
}, {
  monitorChanges: true
});

/**
 * Details about each profile entry cateogry.
 * @see CATEGORY_MAPPINGS.
 */
const CATEGORIES = [{
  color: "#5e88b0",
  abbrev: "other",
  label: L10N.getStr("category.other")
}, {
  color: "#46afe3",
  abbrev: "css",
  label: L10N.getStr("category.css")
}, {
  color: "#d96629",
  abbrev: "js",
  label: L10N.getStr("category.js")
}, {
  color: "#eb5368",
  abbrev: "gc",
  label: L10N.getStr("category.gc")
}, {
  color: "#df80ff",
  abbrev: "network",
  label: L10N.getStr("category.network")
}, {
  color: "#70bf53",
  abbrev: "graphics",
  label: L10N.getStr("category.graphics")
}, {
  color: "#8fa1b2",
  abbrev: "storage",
  label: L10N.getStr("category.storage")
}, {
  color: "#d99b28",
  abbrev: "events",
  label: L10N.getStr("category.events")
}];

/**
 * Mapping from category bitmasks in the profiler data to additional details.
 * To be kept in sync with the js::ProfileEntry::Category in ProfilingStack.h
 */
const CATEGORY_MAPPINGS = {
  "16": CATEGORIES[0],    // js::ProfileEntry::Category::OTHER
  "32": CATEGORIES[1],    // js::ProfileEntry::Category::CSS
  "64": CATEGORIES[2],    // js::ProfileEntry::Category::JS
  "128": CATEGORIES[3],   // js::ProfileEntry::Category::GC
  "256": CATEGORIES[3],   // js::ProfileEntry::Category::CC
  "512": CATEGORIES[4],   // js::ProfileEntry::Category::NETWORK
  "1024": CATEGORIES[5],  // js::ProfileEntry::Category::GRAPHICS
  "2048": CATEGORIES[6],  // js::ProfileEntry::Category::STORAGE
  "4096": CATEGORIES[7],  // js::ProfileEntry::Category::EVENTS
};

/**
 * A simple schema for mapping markers to the timeline UI. The keys correspond
 * to marker names, while the values are objects with the following format:
 *
 * - group: The row index in the timeline overview graph; multiple markers
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
 * - collapseFunc: A function determining how markers are collapsed together.
 *                 Invoked with 3 arguments: the current parent marker, the
 *                 current marker and a method for peeking i markers ahead. If
 *                 nothing is returned, the marker is added as a standalone entry
 *                 in the waterfall. Otherwise, an object needs to be returned
 *                 with the following properties:
 *                 - toParent: The parent marker name (needs to be an entry in
 *                             the `TIMELINE_BLUEPRINT` itself).
 *                 - withData: An object containing some properties to staple
 *                             on the parent marker.
 *                 - forceNew: True if a new parent marker needs to be created
 *                             even though there is one currently available
 *                             with the same name.
 *                 - forceEnd: True if the current parent marker is full after
 *                             this collapse operation and should be finalized.
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
  /* Group 0 - Reflow and Rendering pipeline */
  "Styles": {
    group: 0,
    colorName: "graphs-purple",
    collapseFunc: collapseConsecutiveIdentical,
    label: L10N.getStr("timeline.label.styles2"),
    fields: getStylesFields,
  },
  "Reflow": {
    group: 0,
    colorName: "graphs-purple",
    collapseFunc: collapseConsecutiveIdentical,
    label: L10N.getStr("timeline.label.reflow2"),
  },
  "Paint": {
    group: 0,
    colorName: "graphs-green",
    collapseFunc: collapseConsecutiveIdentical,
    label: L10N.getStr("timeline.label.paint"),
  },

  /* Group 1 - JS */
  "DOMEvent": {
    group: 1,
    colorName: "graphs-yellow",
    collapseFunc: collapseDOMIntoDOMJS,
    label: L10N.getStr("timeline.label.domevent"),
    fields: getDOMEventFields,
  },
  "Javascript": {
    group: 1,
    colorName: "graphs-yellow",
    collapseFunc: either(collapseJSIntoDOMJS, collapseConsecutiveIdentical),
    label: getJSLabel,
    fields: getJSFields,
  },
  "meta::DOMEvent+JS": {
    colorName: "graphs-yellow",
    label: getDOMJSLabel,
    fields: getDOMEventFields,
  },
  "Parse HTML": {
    group: 1,
    colorName: "graphs-yellow",
    collapseFunc: collapseConsecutiveIdentical,
    label: L10N.getStr("timeline.label.parseHTML"),
  },
  "Parse XML": {
    group: 1,
    colorName: "graphs-yellow",
    collapseFunc: collapseConsecutiveIdentical,
    label: L10N.getStr("timeline.label.parseXML"),
  },
  "GarbageCollection": {
    group: 1,
    colorName: "graphs-red",
    collapseFunc: collapseAdjacentGC,
    label: getGCLabel,
    fields: [
      { property: "causeName", label: "Reason:" },
      { property: "nonincrementalReason", label: "Non-incremental Reason:" }
    ],
  },

  /* Group 2 - User Controlled */
  "ConsoleTime": {
    group: 2,
    colorName: "graphs-grey",
    label: sublabelForProperty(L10N.getStr("timeline.label.consoleTime"), "causeName"),
    fields: [{
      property: "causeName",
      label: L10N.getStr("timeline.markerDetail.consoleTimerName")
    }],
  },
  "TimeStamp": {
    group: 2,
    colorName: "graphs-blue",
    label: sublabelForProperty(L10N.getStr("timeline.label.timestamp"), "causeName"),
    fields: [{
      property: "causeName",
      label: "Label:"
    }],
  },
};

/**
 * Helper for creating a function that returns the first defined result from
 * a list of functions passed in as params, in order.
 * @param ...function fun
 * @return any
 */
function either(...fun) {
  return function() {
    for (let f of fun) {
      let result = f.apply(null, arguments);
      if (result !== undefined) return result;
    }
  }
}

/**
 * A series of collapsers used by the blueprint. These functions are
 * consecutively invoked on a moving window of two markers.
 */

function collapseConsecutiveIdentical(parent, curr, peek) {
  // If there is a parent marker currently being filled and the current marker
  // should go into the parent marker, make it so.
  if (parent && parent.name == curr.name) {
    return { toParent: parent.name };
  }
  // Otherwise if the current marker is the same type as the next marker type,
  // create a new parent marker containing the current marker.
  let next = peek(1);
  if (next && curr.name == next.name) {
    return { toParent: curr.name };
  }
}

function collapseAdjacentGC(parent, curr, peek) {
  let next = peek(1);
  if (next && (next.start < curr.end || next.start - curr.end <= 10 /* ms */)) {
    return collapseConsecutiveIdentical(parent, curr, peek);
  }
}

function collapseDOMIntoDOMJS(parent, curr, peek) {
  // If the next marker is a JavaScript marker, create a new meta parent marker
  // containing the current marker.
  let next = peek(1);
  if (next && next.name == "Javascript") {
    return {
      forceNew: true,
      toParent: "meta::DOMEvent+JS",
      withData: {
        type: curr.type,
        eventPhase: curr.eventPhase
      },
    };
  }
}

function collapseJSIntoDOMJS(parent, curr, peek) {
  // If there is a parent marker currently being filled, and it's the one
  // created from a `DOMEvent` via `collapseDOMIntoDOMJS`, then the current
  // marker has to go into that one.
  if (parent && parent.name == "meta::DOMEvent+JS") {
    return {
      forceEnd: true,
      toParent: "meta::DOMEvent+JS",
      withData: {
        stack: curr.stack,
        endStack: curr.endStack
      },
    };
  }
}

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
  // The difference between these two event handler markers are differences
  // in their WebIDL implementation, so distinguishing them is not necessary.
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

function getDOMJSLabel (marker={}) {
  return `Event (${marker.type})`;
}

/**
 * Returns a hash for computing a fields object for a JS marker. If the cause
 * is considered content (so an entry exists in the JS_MARKER_MAP), do not display it
 * since it's redundant with the label. Otherwise for Gecko code, either display
 * the cause, or "(Gecko)", depending on if "show-platform-data" is set.
 */
function getJSFields (marker) {
  if ("causeName" in marker && !JS_MARKER_MAP[marker.causeName]) {
    return { Reason: PREFS["show-platform-data"] ? marker.causeName : GECKO_SYMBOL };
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

/**
 * Get the numeric bitmask (or set of masks) for the given category
 * abbreviation. See CATEGORIES and CATEGORY_MAPPINGS above.
 *
 * CATEGORY_MASK can be called with just a name if it is expected that the
 * category is mapped to by exactly one bitmask.  If the category is mapped
 * to by multiple masks, CATEGORY_MASK for that name must be called with
 * an additional argument specifying the desired id (in ascending order).
 */
const [CATEGORY_MASK, CATEGORY_MASK_LIST] = (function () {
  let bitmasksForCategory = {};
  let all = Object.keys(CATEGORY_MAPPINGS);

  for (let category of CATEGORIES) {
    bitmasksForCategory[category.abbrev] = all
      .filter(mask => CATEGORY_MAPPINGS[mask] == category)
      .map(mask => +mask)
      .sort();
  }

  return [
    function (name, index) {
      if (!(name in bitmasksForCategory)) {
        throw new Error(`Category abbreviation '${name}' does not exist.`);
      }
      if (arguments.length == 1) {
        if (bitmasksForCategory[name].length != 1) {
          throw new Error(`Expected exactly one category number for '${name}'.`);
        } else {
          return bitmasksForCategory[name][0];
        }
      } else {
        if (index > bitmasksForCategory[name].length) {
          throw new Error(`Index '${index}' too high for category '${name}'.`);
        } else {
          return bitmasksForCategory[name][index - 1];
        }
      }
    },

    function (name) {
      if (!(name in bitmasksForCategory)) {
        throw new Error(`Category abbreviation '${name}' does not exist.`);
      }
      return bitmasksForCategory[name];
    }
  ];
})();

// Human-readable "other" category bitmask. Older Geckos don't have all the
// necessary instrumentation in the sampling profiler backend for creating
// a categories graph, in which case we default to the "other" category.
const CATEGORY_OTHER = CATEGORY_MASK('other');

// Human-readable JIT category bitmask. Certain pseudo-frames in a sample,
// like "EnterJIT", don't have any associated `cateogry` information.
const CATEGORY_JIT = CATEGORY_MASK('js');

// Exported symbols.
exports.L10N = L10N;
exports.PREFS = PREFS;
exports.TIMELINE_BLUEPRINT = TIMELINE_BLUEPRINT;
exports.CATEGORIES = CATEGORIES;
exports.CATEGORY_MAPPINGS = CATEGORY_MAPPINGS;
exports.CATEGORY_MASK = CATEGORY_MASK;
exports.CATEGORY_MASK_LIST = CATEGORY_MASK_LIST;
exports.CATEGORY_OTHER = CATEGORY_OTHER;
exports.CATEGORY_JIT = CATEGORY_JIT;
