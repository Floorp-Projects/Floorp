/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file contains utilities for creating DOM nodes for markers
 * to be displayed in the UI.
 */

const { L10N, PREFS } = require("devtools/client/performance/modules/global");
const { MarkerBlueprintUtils } = require("devtools/client/performance/modules/marker-blueprint-utils");
const { getSourceNames } = require("devtools/client/shared/source-utils");

/**
 * Utilites for creating elements for markers.
 */
exports.MarkerDOMUtils = {
  /**
   * Builds all the fields possible for the given marker. Returns an
   * array of elements to be appended to a parent element.
   *
   * @param document doc
   * @param object marker
   * @return array<Node>
   */
  buildFields: function(doc, marker) {
    const fields = MarkerBlueprintUtils.getMarkerFields(marker);
    return fields.map(({ label, value }) => this.buildNameValueLabel(doc, label, value));
  },

  /**
   * Builds the label representing the marker's type.
   *
   * @param document doc
   * @param object marker
   * @return Node
   */
  buildTitle: function(doc, marker) {
    const blueprint = MarkerBlueprintUtils.getBlueprintFor(marker);

    const hbox = doc.createElement("hbox");
    hbox.setAttribute("align", "center");

    const bullet = doc.createElement("hbox");
    bullet.className = `marker-details-bullet marker-color-${blueprint.colorName}`;

    const title = MarkerBlueprintUtils.getMarkerLabel(marker);
    const label = doc.createElement("label");
    label.className = "marker-details-type";
    label.setAttribute("value", title);

    hbox.appendChild(bullet);
    hbox.appendChild(label);

    return hbox;
  },

  /**
   * Builds the label representing the marker's duration.
   *
   * @param document doc
   * @param object marker
   * @return Node
   */
  buildDuration: function(doc, marker) {
    const label = L10N.getStr("marker.field.duration");
    const start = L10N.getFormatStrWithNumbers("timeline.tick", marker.start);
    const end = L10N.getFormatStrWithNumbers("timeline.tick", marker.end);
    const duration = L10N.getFormatStrWithNumbers("timeline.tick",
                                                marker.end - marker.start);

    const el = this.buildNameValueLabel(doc, label, duration);
    el.classList.add("marker-details-duration");
    el.setAttribute("tooltiptext", `${start} → ${end}`);

    return el;
  },

  /**
   * Builds labels for name:value pairs.
   * E.g. "Start: 100ms", "Duration: 200ms", ...
   *
   * @param document doc
   * @param string field
   * @param string value
   * @return Node
   */
  buildNameValueLabel: function(doc, field, value) {
    const hbox = doc.createElement("hbox");
    hbox.className = "marker-details-labelcontainer";

    const nameLabel = doc.createElement("label");
    nameLabel.className = "plain marker-details-name-label";
    nameLabel.setAttribute("value", field);
    hbox.appendChild(nameLabel);

    const valueLabel = doc.createElement("label");
    valueLabel.className = "plain marker-details-value-label";
    valueLabel.setAttribute("value", value);
    hbox.appendChild(valueLabel);

    return hbox;
  },

  /**
   * Builds a stack trace in an element.
   *
   * @param document doc
   * @param object params
   *        An options object with the following members:
   *          - string type: string identifier for type of stack ("stack", "startStack"
                             or "endStack"
   *          - number frameIndex: the index of the topmost stack frame
   *          - array frames: array of stack frames
   */
  buildStackTrace: function(doc, { type, frameIndex, frames }) {
    const container = doc.createElement("vbox");
    container.className = "marker-details-stack";
    container.setAttribute("type", type);

    const nameLabel = doc.createElement("label");
    nameLabel.className = "plain marker-details-name-label";
    nameLabel.setAttribute("value", L10N.getStr(`marker.field.${type}`));
    container.appendChild(nameLabel);

    // Workaround for profiles that have looping stack traces.  See
    // bug 1246555.
    let wasAsyncParent = false;
    const seen = new Set();

    while (frameIndex > 0) {
      if (seen.has(frameIndex)) {
        break;
      }
      seen.add(frameIndex);

      const frame = frames[frameIndex];
      const url = frame.source;
      const displayName = frame.functionDisplayName;
      const line = frame.line;

      // If the previous frame had an async parent, then the async
      // cause is in this frame and should be displayed.
      if (wasAsyncParent) {
        const asyncStr = L10N.getFormatStr("marker.field.asyncStack", frame.asyncCause);
        const asyncBox = doc.createElement("hbox");
        const asyncLabel = doc.createElement("label");
        asyncLabel.className = "devtools-monospace";
        asyncLabel.setAttribute("value", asyncStr);
        asyncBox.appendChild(asyncLabel);
        container.appendChild(asyncBox);
        wasAsyncParent = false;
      }

      const hbox = doc.createElement("hbox");

      if (displayName) {
        const functionLabel = doc.createElement("label");
        functionLabel.className = "devtools-monospace";
        functionLabel.setAttribute("value", displayName);
        hbox.appendChild(functionLabel);
      }

      if (url) {
        const linkNode = doc.createElement("a");
        linkNode.className = "waterfall-marker-location devtools-source-link";
        linkNode.href = url;
        linkNode.draggable = false;
        linkNode.setAttribute("title", url);

        const urlLabel = doc.createElement("label");
        urlLabel.className = "filename";
        urlLabel.setAttribute("value", getSourceNames(url).short);
        linkNode.appendChild(urlLabel);

        const lineLabel = doc.createElement("label");
        lineLabel.className = "line-number";
        lineLabel.setAttribute("value", `:${line}`);
        linkNode.appendChild(lineLabel);

        hbox.appendChild(linkNode);

        // Clicking here will bubble up to the parent,
        // which handles the view source.
        linkNode.setAttribute("data-action", JSON.stringify({
          url: url,
          line: line,
          action: "view-source"
        }));
      }

      if (!displayName && !url) {
        const unknownLabel = doc.createElement("label");
        unknownLabel.setAttribute("value", L10N.getStr("marker.value.unknownFrame"));
        hbox.appendChild(unknownLabel);
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
  },

  /**
   * Builds any custom fields specific to the marker.
   *
   * @param document doc
   * @param object marker
   * @param object options
   * @return array<Node>
   */
  buildCustom: function(doc, marker, options) {
    const elements = [];

    if (options.allocations && shouldShowAllocationsTrigger(marker)) {
      const hbox = doc.createElement("hbox");
      hbox.className = "marker-details-customcontainer";

      const label = doc.createElement("label");
      label.className = "custom-button";
      label.setAttribute("value", "Show allocation triggers");
      label.setAttribute("type", "show-allocations");
      label.setAttribute("data-action", JSON.stringify({
        endTime: marker.start,
        action: "show-allocations"
      }));

      hbox.appendChild(label);
      elements.push(hbox);
    }

    return elements;
  },
};

/**
 * Takes a marker and determines if this marker should display
 * the allocations trigger button.
 *
 * @param object marker
 * @return boolean
 */
function shouldShowAllocationsTrigger(marker) {
  if (marker.name == "GarbageCollection") {
    const showTriggers = PREFS["show-triggers-for-gc-types"];
    return showTriggers.split(" ").includes(marker.causeName);
  }
  return false;
}
