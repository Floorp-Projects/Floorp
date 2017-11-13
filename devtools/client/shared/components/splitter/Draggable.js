/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const React = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { Component, DOM: dom, PropTypes } = React;

class Draggable extends Component {
  static get propTypes() {
    return {
      onMove: PropTypes.func.isRequired,
      onStart: PropTypes.func,
      onStop: PropTypes.func,
      style: PropTypes.object,
      className: PropTypes.string
    };
  }

  constructor(props) {
    super(props);
    this.startDragging = this.startDragging.bind(this);
    this.onMove = this.onMove.bind(this);
    this.onUp = this.onUp.bind(this);
  }

  startDragging(ev) {
    ev.preventDefault();
    const doc = ReactDOM.findDOMNode(this).ownerDocument;
    doc.addEventListener("mousemove", this.onMove);
    doc.addEventListener("mouseup", this.onUp);
    this.props.onStart && this.props.onStart();
  }

  onMove(ev) {
    ev.preventDefault();
    // Use viewport coordinates so, moving mouse over iframes
    // doesn't mangle (relative) coordinates.
    this.props.onMove(ev.clientX, ev.clientY);
  }

  onUp(ev) {
    ev.preventDefault();
    const doc = ReactDOM.findDOMNode(this).ownerDocument;
    doc.removeEventListener("mousemove", this.onMove);
    doc.removeEventListener("mouseup", this.onUp);
    this.props.onStop && this.props.onStop();
  }

  render() {
    return dom.div({
      style: this.props.style,
      className: this.props.className,
      onMouseDown: this.startDragging
    });
  }
}

module.exports = Draggable;
