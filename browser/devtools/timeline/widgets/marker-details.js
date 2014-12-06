/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let { Ci } = require("chrome");

/**
 * This file contains the rendering code for the marker sidebar.
 */

loader.lazyRequireGetter(this, "L10N",
  "devtools/timeline/global", true);
loader.lazyRequireGetter(this, "TIMELINE_BLUEPRINT",
  "devtools/timeline/global", true);
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
    bullet.className = "marker-details-bullet";
    bullet.style.backgroundColor = blueprint.fill;
    bullet.style.borderColor = blueprint.stroke;

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
   * @param object marker
   *        The marker to display.
   */
  render: function(marker) {
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
      default:
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

}

exports.MarkerDetails = MarkerDetails;
