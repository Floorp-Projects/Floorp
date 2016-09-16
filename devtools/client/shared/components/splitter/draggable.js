/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const React = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { DOM: dom, PropTypes } = React;

const Draggable = React.createClass({
  displayName: "Draggable",

  propTypes: {
    onMove: PropTypes.func.isRequired,
    onStart: PropTypes.func,
    onStop: PropTypes.func,
    style: PropTypes.object,
    className: PropTypes.string
  },

  startDragging(ev) {
    ev.preventDefault();
    const doc = ReactDOM.findDOMNode(this).ownerDocument;
    doc.addEventListener("mousemove", this.onMove);
    doc.addEventListener("mouseup", this.onUp);
    this.props.onStart && this.props.onStart();
  },

  onMove(ev) {
    ev.preventDefault();
    // Use screen coordinates so, moving mouse over iframes
    // doesn't mangle (relative) coordinates.
    this.props.onMove(ev.screenX, ev.screenY);
  },

  onUp(ev) {
    ev.preventDefault();
    const doc = ReactDOM.findDOMNode(this).ownerDocument;
    doc.removeEventListener("mousemove", this.onMove);
    doc.removeEventListener("mouseup", this.onUp);
    this.props.onStop && this.props.onStop();
  },

  render() {
    return dom.div({
      style: this.props.style,
      className: this.props.className,
      onMouseDown: this.startDragging
    });
  }
});

module.exports = Draggable;
