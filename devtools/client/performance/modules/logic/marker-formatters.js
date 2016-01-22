/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file contains utilities for creating elements for markers to be displayed,
 * and parsing out the blueprint to generate correct values for markers.
 */
const { Ci } = require("chrome");
const Services = require("Services");
const { L10N } = require("devtools/client/performance/modules/global");
const PLATFORM_DATA_PREF = "devtools.performance.ui.show-platform-data";

// String used to fill in platform data when it should be hidden.
const GECKO_SYMBOL = "(Gecko)";

/**
 * Mapping of JS marker causes to a friendlier form. Only
 * markers that are considered "from content" should be labeled here.
 */
const JS_MARKER_MAP = {
  "<script> element":          L10N.getStr("marker.label.javascript.scriptElement"),
  "promise callback":          L10N.getStr("marker.label.javascript.promiseCallback"),
  "promise initializer":       L10N.getStr("marker.label.javascript.promiseInit"),
  "Worker runnable":           L10N.getStr("marker.label.javascript.workerRunnable"),
  "javascript: URI":           L10N.getStr("marker.label.javascript.jsURI"),
  // The difference between these two event handler markers are differences
  // in their WebIDL implementation, so distinguishing them is not necessary.
  "EventHandlerNonNull":       L10N.getStr("marker.label.javascript.eventHandler"),
  "EventListener.handleEvent": L10N.getStr("marker.label.javascript.eventHandler"),
  // These markers do not get L10N'd because they're JS names.
  "setInterval handler":       "setInterval",
  "setTimeout handler":        "setTimeout",
  "FrameRequestCallback":      "requestAnimationFrame",
};

/**
 * A series of formatters used by the blueprint.
 */
const Formatters = {
  /**
   * Uses the marker name as the label for markers that do not have
   * a blueprint entry. Uses "Other" in the marker filter menu.
   */
  UnknownLabel: function (marker={}) {
    return marker.name || L10N.getStr("marker.label.unknown");
  },

  GCLabel: function (marker) {
    if (!marker) {
      return L10N.getStr("marker.label.garbageCollection2");
    }
    // Only if a `nonincrementalReason` exists, do we want to label
    // this as a non incremental GC event.
    if ("nonincrementalReason" in marker) {
      return L10N.getStr("marker.label.garbageCollection.nonIncremental");
    }
    return L10N.getStr("marker.label.garbageCollection.incremental");
  },

  JSLabel: function (marker={}) {
    let generic = L10N.getStr("marker.label.javascript");
    if ("causeName" in marker) {
      return JS_MARKER_MAP[marker.causeName] || generic;
    }
    return generic;
  },

  DOMJSLabel: function (marker={}) {
    return `Event (${marker.type})`;
  },

  /**
   * Returns a hash for computing a fields object for a JS marker. If the cause
   * is considered content (so an entry exists in the JS_MARKER_MAP), do not display it
   * since it's redundant with the label. Otherwise for Gecko code, either display
   * the cause, or "(Gecko)", depending on if "show-platform-data" is set.
   */
  JSFields: function (marker) {
    if ("causeName" in marker && !JS_MARKER_MAP[marker.causeName]) {
      let cause = Services.prefs.getBoolPref(PLATFORM_DATA_PREF) ? marker.causeName : GECKO_SYMBOL;
      return {
        [L10N.getStr("marker.field.causeName")]: cause
      };
    }
  },

  GCFields: function (marker) {
    let fields = Object.create(null);
    let cause = marker.causeName;
    let label = L10N.getStr(`marker.gcreason.label.${cause}`) || cause;

    fields[L10N.getStr("marker.field.causeName")] = label;

    if ("nonincrementalReason" in marker) {
      fields[L10N.getStr("marker.field.nonIncrementalCause")] = marker.nonincrementalReason;
    }

    return fields;
  },

  MinorGCFields: function (marker) {
    const cause = marker.causeName;
    const label = L10N.getStr(`marker.gcreason.label.${cause}`) || cause;
    return {
      [L10N.getStr("marker.field.type")]: L10N.getStr("marker.nurseryCollection"),
      [L10N.getStr("marker.field.causeName")]: label,
    };
  },

  DOMEventFields: function (marker) {
    let fields = Object.create(null);
    if ("type" in marker) {
      fields[L10N.getStr("marker.field.DOMEventType")] = marker.type;
    }
    if ("eventPhase" in marker) {
      let phase;
      if (marker.eventPhase === Ci.nsIDOMEvent.AT_TARGET) {
        phase = L10N.getStr("marker.value.DOMEventTargetPhase");
      } else if (marker.eventPhase === Ci.nsIDOMEvent.CAPTURING_PHASE) {
        phase = L10N.getStr("marker.value.DOMEventCapturingPhase");
      } else if (marker.eventPhase === Ci.nsIDOMEvent.BUBBLING_PHASE) {
        phase = L10N.getStr("marker.value.DOMEventBubblingPhase");
      }
      fields[L10N.getStr("marker.field.DOMEventPhase")] = phase;
    }
    return fields;
  },

  StylesFields: function (marker) {
    if ("restyleHint" in marker) {
      return {
        [L10N.getStr("marker.field.restyleHint")]: marker.restyleHint.replace(/eRestyle_/g, "")
      };
    }
  },

  CycleCollectionFields: function (marker) {
    return {
      [L10N.getStr("marker.field.type")]: marker.name.replace(/nsCycleCollector::/g, "")
    };
  },

  WorkerFields: function(marker) {
    return {
      [L10N.getStr("marker.field.type")]:
        L10N.getStr(`marker.worker.${marker.workerOperation}`)
    };
  }
};

exports.Formatters = Formatters;
