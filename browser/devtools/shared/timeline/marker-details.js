/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

let { Ci } = require("chrome");
let WebConsoleUtils = require("devtools/toolkit/webconsole/utils").Utils;

/**
 * This file contains the rendering code for the marker sidebar.
 */

loader.lazyRequireGetter(this, "L10N",
  "devtools/shared/timeline/global", true);
loader.lazyRequireGetter(this, "TIMELINE_BLUEPRINT",
  "devtools/shared/timeline/global", true);
loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");

/**
 * A detailed view for one single marker.
 *
 * @param nsIDOMNode parent
 *        The parent node holding the view.
 * @param nsIDOMNode splitter
 *        The splitter node that the resize event is bound to.
 */
function MarkerDetails(parent, splitter) {
  EventEmitter.decorate(this);
  this._document = parent.ownerDocument;
  this._parent = parent;
  this._splitter = splitter;
  this._splitter.addEventListener("mouseup", () => this.emit("resize"));
}

MarkerDetails.prototype = {
  /**
   * Removes any node references from this view.
   */
  destroy: function() {
    this.empty();
    this._parent = null;
    this._splitter = null;
  },

  /**
   * Clears the view.
   */
  empty: function() {
    this._parent.innerHTML = "";
  },

  /**
   * Builds the label representing marker's type.
   *
   * @param string type
   *        Could be "Paint", "Reflow", "Styles", ...
   *        See TIMELINE_BLUEPRINT in widgets/global.js
   */
  buildMarkerTypeLabel: function(type) {
    let blueprint = TIMELINE_BLUEPRINT[type];

    let hbox = this._document.createElement("hbox");
    hbox.setAttribute("align", "center");

    let bullet = this._document.createElement("hbox");
    bullet.className = `marker-details-bullet ${blueprint.colorName}`;

    let label = this._document.createElement("label");
    label.className = "marker-details-type";
    label.setAttribute("value", blueprint.label);

    hbox.appendChild(bullet);
    hbox.appendChild(label);

    return hbox;
  },

  /**
   * Builds labels for name:value pairs. Like "Start: 100ms",
   * "Duration: 200ms", ...
   *
   * @param string l10nName
   *        String identifier for label's name.
   * @param string value
   *        Label's value.
   */
  buildNameValueLabel: function(l10nName, value) {
    let hbox = this._document.createElement("hbox");
    let labelName = this._document.createElement("label");
    let labelValue = this._document.createElement("label");
    labelName.className = "plain marker-details-labelname";
    labelValue.className = "plain marker-details-labelvalue";
    labelName.setAttribute("value", L10N.getStr(l10nName));
    labelValue.setAttribute("value", value);
    hbox.appendChild(labelName);
    hbox.appendChild(labelValue);
    return hbox;
  },

  /**
   * Populates view with marker's details.
   *
   * @param object params
   *        An options object holding:
   *        toolbox - The toolbox.
   *        marker - The marker to display.
   *        frames - Array of stack frame information; see stack.js.
   */
  render: function({toolbox: toolbox, marker: marker, frames: frames}) {
    this.empty();

    // UI for any marker

    let title = this.buildMarkerTypeLabel(marker.name);

    let toMs = ms => L10N.getFormatStrWithNumbers("timeline.tick", ms);

    let start = this.buildNameValueLabel("timeline.markerDetail.start", toMs(marker.start));
    let end = this.buildNameValueLabel("timeline.markerDetail.end", toMs(marker.end));
    let duration = this.buildNameValueLabel("timeline.markerDetail.duration", toMs(marker.end - marker.start));

    start.classList.add("marker-details-start");
    end.classList.add("marker-details-end");
    duration.classList.add("marker-details-duration");

    this._parent.appendChild(title);
    this._parent.appendChild(start);
    this._parent.appendChild(end);
    this._parent.appendChild(duration);

    // UI for specific markers

    switch (marker.name) {
      case "ConsoleTime":
        this.renderConsoleTimeMarker(this._parent, marker);
        break;
      case "DOMEvent":
        this.renderDOMEventMarker(this._parent, marker);
        break;
      case "Javascript":
        this.renderJavascriptMarker(this._parent, marker);
        break;
      default:
    }

    if (marker.stack) {
      let property = "timeline.markerDetail.stack";
      if (marker.endStack) {
        property = "timeline.markerDetail.startStack";
      }
      this.renderStackTrace({toolbox: toolbox, parent: this._parent, property: property,
                             frameIndex: marker.stack, frames: frames});
    }

    if (marker.endStack) {
      this.renderStackTrace({toolbox: toolbox, parent: this._parent, property: "timeline.markerDetail.endStack",
                             frameIndex: marker.endStack, frames: frames});
    }
  },

  /**
   * Render a stack trace.
   *
   * @param object params
   *        An options object with the following members:
   *        object toolbox - The toolbox.
   *        nsIDOMNode parent - The parent node holding the view.
   *        string property - String identifier for label's name.
   *        integer frameIndex - The index of the topmost stack frame.
   *        array frames - Array of stack frames.
   */
  renderStackTrace: function({toolbox: toolbox, parent: parent,
                              property: property, frameIndex: frameIndex,
                              frames: frames}) {
    let labelName = this._document.createElement("label");
    labelName.className = "plain marker-details-labelname";
    labelName.setAttribute("value", L10N.getStr(property));
    parent.appendChild(labelName);

    let wasAsyncParent = false;
    while (frameIndex > 0) {
      let frame = frames[frameIndex];
      let url = frame.source;
      let displayName = frame.functionDisplayName;
      let line = frame.line;

      // If the previous frame had an async parent, then the async
      // cause is in this frame and should be displayed.
      if (wasAsyncParent) {
        let asyncBox = this._document.createElement("hbox");
        let asyncLabel = this._document.createElement("label");
        asyncLabel.className = "devtools-monospace";
        asyncLabel.setAttribute("value", L10N.getFormatStr("timeline.markerDetail.asyncStack",
                                                           frame.asyncCause));
        asyncBox.appendChild(asyncLabel);
        parent.appendChild(asyncBox);
        wasAsyncParent = false;
      }

      let hbox = this._document.createElement("hbox");

      if (displayName) {
        let functionLabel = this._document.createElement("label");
        functionLabel.className = "devtools-monospace";
        functionLabel.setAttribute("value", displayName);
        hbox.appendChild(functionLabel);
      }

      if (url) {
        let aNode = this._document.createElement("a");
        aNode.className = "waterfall-marker-location theme-link devtools-monospace";
        aNode.href = url;
        aNode.draggable = false;
        aNode.setAttribute("title", url);

        let text = WebConsoleUtils.abbreviateSourceURL(url) + ":" + line;
        let label = this._document.createElement("label");
        label.setAttribute("value", text);
        aNode.appendChild(label);
        hbox.appendChild(aNode);

        aNode.addEventListener("click", (event) => {
          event.preventDefault();
          this.emit("view-source", url, line);
        });
      }

      if (!displayName && !url) {
        let label = this._document.createElement("label");
        label.setAttribute("value", L10N.getStr("timeline.markerDetail.unknownFrame"));
        hbox.appendChild(label);
      }

      parent.appendChild(hbox);

      if (frame.asyncParent) {
        frameIndex = frame.asyncParent;
        wasAsyncParent = true;
      } else {
        frameIndex = frame.parent;
      }
    }
  },

  /**
   * Render details of a console marker (console.time).
   *
   * @param nsIDOMNode parent
   *        The parent node holding the view.
   * @param object marker
   *        The marker to display.
   */
  renderConsoleTimeMarker: function(parent, marker) {
    if ("causeName" in marker) {
      let timerName = this.buildNameValueLabel("timeline.markerDetail.consoleTimerName", marker.causeName);
      this._parent.appendChild(timerName);
    }
  },

  /**
   * Render details of a DOM Event marker.
   *
   * @param nsIDOMNode parent
   *        The parent node holding the view.
   * @param object marker
   *        The marker to display.
   */
  renderDOMEventMarker: function(parent, marker) {
    if ("type" in marker) {
      let type = this.buildNameValueLabel("timeline.markerDetail.DOMEventType", marker.type);
      this._parent.appendChild(type);
    }
    if ("eventPhase" in marker) {
      let phaseL10NProp;
      if (marker.eventPhase == Ci.nsIDOMEvent.AT_TARGET) {
        phaseL10NProp = "timeline.markerDetail.DOMEventTargetPhase";
      }
      if (marker.eventPhase == Ci.nsIDOMEvent.CAPTURING_PHASE) {
        phaseL10NProp = "timeline.markerDetail.DOMEventCapturingPhase";
      }
      if (marker.eventPhase == Ci.nsIDOMEvent.BUBBLING_PHASE) {
        phaseL10NProp = "timeline.markerDetail.DOMEventBubblingPhase";
      }
      let phase = this.buildNameValueLabel("timeline.markerDetail.DOMEventPhase", L10N.getStr(phaseL10NProp));
      this._parent.appendChild(phase);
    }
  },

  /**
   * Render details of a Javascript marker.
   *
   * @param nsIDOMNode parent
   *        The parent node holding the view.
   * @param object marker
   *        The marker to display.
   */
  renderJavascriptMarker: function(parent, marker) {
    if ("causeName" in marker) {
      let cause = this.buildNameValueLabel("timeline.markerDetail.causeName", marker.causeName);
      this._parent.appendChild(cause);
    }
  },

};

exports.MarkerDetails = MarkerDetails;
