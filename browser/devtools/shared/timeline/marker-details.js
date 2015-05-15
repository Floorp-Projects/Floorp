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
loader.lazyRequireGetter(this, "MarkerUtils",
  "devtools/shared/timeline/marker-utils");

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

    let title = MarkerUtils.DOM.buildTitle(this._document, marker);
    let duration = MarkerUtils.DOM.buildDuration(this._document, marker);
    let fields = MarkerUtils.DOM.buildFields(this._document, marker);

    this._parent.appendChild(title);
    this._parent.appendChild(duration);
    fields.forEach(field => this._parent.appendChild(field));

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
};

exports.MarkerDetails = MarkerDetails;
