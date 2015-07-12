/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file contains utilities for creating elements for markers to be displayed,
 * and parsing out the blueprint to generate correct values for markers.
 */

const { Cu, Ci } = require("chrome");

loader.lazyRequireGetter(this, "L10N",
  "devtools/performance/global", true);
loader.lazyRequireGetter(this, "PREFS",
  "devtools/performance/global", true);
loader.lazyRequireGetter(this, "TIMELINE_BLUEPRINT",
  "devtools/performance/markers", true);
loader.lazyRequireGetter(this, "WebConsoleUtils",
  "devtools/toolkit/webconsole/utils");

// String used to fill in platform data when it should be hidden.
const GECKO_SYMBOL = "(Gecko)";

/**
 * Takes a marker, blueprint, and filter list and
 * determines if this marker should be filtered or not.
 */
function isMarkerValid (marker, filter) {
  if (!filter || filter.length === 0) {
    return true;
  }

  let isUnknown = !(marker.name in TIMELINE_BLUEPRINT);
  if (isUnknown) {
    return filter.indexOf("UNKNOWN") === -1;
  }
  return filter.indexOf(marker.name) === -1;
}

/**
 * Returns the correct label to display for passed in marker, based
 * off of the blueprints.
 *
 * @param {ProfileTimelineMarker} marker
 * @return {string}
 */
function getMarkerLabel (marker) {
  let blueprint = getBlueprintFor(marker);
  // Either use the label function in the blueprint, or use it directly
  // as a string.
  return typeof blueprint.label === "function" ? blueprint.label(marker) : blueprint.label;
}

/**
 * Returns the correct generic name for a marker class, like "Function Call"
 * being the general class for JS markers, rather than "setTimeout", etc.
 *
 * @param {string} type
 * @return {string}
 */
function getMarkerClassName (type) {
  let blueprint = getBlueprintFor({ name: type });
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

/**
 * Returns an array of objects with key/value pairs of what should be rendered
 * in the marker details view.
 *
 * @param {ProfileTimelineMarker} marker
 * @return {Array<object>}
 */
function getMarkerFields (marker) {
  let blueprint = getBlueprintFor(marker);

  // If blueprint.fields is a function, use that
  if (typeof blueprint.fields === "function") {
    let fields = blueprint.fields(marker);
    // Add a ":" to the label since the localization files contain the ":"
    // if not present. This should be changed, ugh.
    return Object.keys(fields || []).map(label => {
      // TODO revisit localization strings for markers bug 1163763
      let normalizedLabel = label.indexOf(":") !== -1 ? label : (label + ":");
      return { label: normalizedLabel, value: fields[label] };
    });
  }

  // Otherwise, iterate over the array
  return (blueprint.fields || []).reduce((fields, field) => {
    // Ensure this marker has this field present
    if (field.property in marker) {
      let label = field.label;
      let value = marker[field.property];
      fields.push({ label, value });
    }
    return fields;
  }, []);
}

/**
 * Utilites for creating elements for markers.
 */
const DOM = {
  /**
   * Builds all the fields possible for the given marker. Returns an
   * array of elements to be appended to a parent element.
   *
   * @param {Document} doc
   * @param {ProfileTimelineMarker} marker
   * @return {Array<Element>}
   */
  buildFields: function (doc, marker) {
    let blueprint = getBlueprintFor(marker);
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
    let blueprint = getBlueprintFor(marker);

    let hbox = doc.createElement("hbox");
    hbox.setAttribute("align", "center");

    let bullet = doc.createElement("hbox");
    bullet.className = `marker-details-bullet marker-color-${blueprint.colorName}`;

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
    hbox.className = "marker-details-labelcontainer";
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

  /**
   * Builds a stack trace in an element.
   *
   * @param {Document} doc
   * @param object params
   *        An options object with the following members:
   *        string type - String identifier for type of stack ("stack", "startStack" or "endStack")
   *        number frameIndex - The index of the topmost stack frame.
   *        array frames - Array of stack frames.
   */
  buildStackTrace: function(doc, { type, frameIndex, frames }) {
    let container = doc.createElement("vbox");
    let labelName = doc.createElement("label");
    labelName.className = "plain marker-details-labelname";
    labelName.setAttribute("value", L10N.getStr(`timeline.markerDetail.${type}`));
    container.setAttribute("type", type);
    container.className = "marker-details-stack";
    container.appendChild(labelName);

    let wasAsyncParent = false;
    while (frameIndex > 0) {
      let frame = frames[frameIndex];
      let url = frame.source;
      let displayName = frame.functionDisplayName;
      let line = frame.line;

      // If the previous frame had an async parent, then the async
      // cause is in this frame and should be displayed.
      if (wasAsyncParent) {
        let asyncBox = doc.createElement("hbox");
        let asyncLabel = doc.createElement("label");
        asyncLabel.className = "devtools-monospace";
        asyncLabel.setAttribute("value", L10N.getFormatStr("timeline.markerDetail.asyncStack",
                                                           frame.asyncCause));
        asyncBox.appendChild(asyncLabel);
        container.appendChild(asyncBox);
        wasAsyncParent = false;
      }

      let hbox = doc.createElement("hbox");

      if (displayName) {
        let functionLabel = doc.createElement("label");
        functionLabel.className = "devtools-monospace";
        functionLabel.setAttribute("value", displayName);
        hbox.appendChild(functionLabel);
      }

      if (url) {
        let aNode = doc.createElement("a");
        aNode.className = "waterfall-marker-location devtools-source-link";
        aNode.href = url;
        aNode.draggable = false;
        aNode.setAttribute("title", url);

        let urlNode = doc.createElement("label");
        urlNode.className = "filename";
        urlNode.setAttribute("value", WebConsoleUtils.Utils.abbreviateSourceURL(url));
        let lineNode = doc.createElement("label");
        lineNode.className = "line-number";
        lineNode.setAttribute("value", `:${line}`);

        aNode.appendChild(urlNode);
        aNode.appendChild(lineNode);
        hbox.appendChild(aNode);

        // Clicking here will bubble up to the parent,
        // which handles the view source.
        aNode.setAttribute("data-action", JSON.stringify({
          url, line, action: "view-source"
        }));
      }

      if (!displayName && !url) {
        let label = doc.createElement("label");
        label.setAttribute("value", L10N.getStr("timeline.markerDetail.unknownFrame"));
        hbox.appendChild(label);
      }

      container.appendChild(hbox);

      if (frame.asyncParent) {
        frameIndex = frame.asyncParent;
        wasAsyncParent = true;
      } else {
        frameIndex = frame.parent;
      }
    }

    return container;
  }
};

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

/**
 * A series of formatters used by the blueprint.
 */
const Formatters = {
  /**
   * Uses the marker name as the label for markers that do not have
   * a blueprint entry. Uses "Other" in the marker filter menu.
   */
  UnknownLabel: function (marker={}) {
    return marker.name || L10N.getStr("timeline.label.unknown");
  },

  GCLabel: function (marker={}) {
    let label = L10N.getStr("timeline.label.garbageCollection");
    // Only if a `nonincrementalReason` exists, do we want to label
    // this as a non incremental GC event.
    if ("nonincrementalReason" in marker) {
      label = `${label} (Non-incremental)`;
    }
    return label;
  },

  JSLabel: function (marker={}) {
    let generic = L10N.getStr("timeline.label.javascript2");
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
      return { Reason: PREFS["show-platform-data"] ? marker.causeName : GECKO_SYMBOL };
    }
  },

  DOMEventFields: function (marker) {
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
  },

  StylesFields: function (marker) {
    if ("restyleHint" in marker) {
      return { "Restyle Hint": marker.restyleHint.replace(/eRestyle_/g, "") };
    }
  },

  CycleCollectionFields: function (marker) {
    let Type = PREFS["show-platform-data"]
        ? marker.name
        : marker.name.replace(/nsCycleCollector::/g, "");
    return { Type };
  },
};

/**
 * Takes a marker and returns the definition for that marker type,
 * falling back to the UNKNOWN definition if undefined.
 *
 * @param {Marker} marker
 * @return {object}
 */
function getBlueprintFor (marker) {
  return TIMELINE_BLUEPRINT[marker.name] || TIMELINE_BLUEPRINT.UNKNOWN;
}

exports.isMarkerValid = isMarkerValid;
exports.getMarkerLabel = getMarkerLabel;
exports.getMarkerClassName = getMarkerClassName;
exports.getMarkerFields = getMarkerFields;
exports.DOM = DOM;
exports.Formatters = Formatters;
exports.getBlueprintFor = getBlueprintFor;
