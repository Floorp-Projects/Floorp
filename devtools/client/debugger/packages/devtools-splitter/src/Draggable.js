/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const React = require("react");
const ReactDOM = require("react-dom");
const { Component } = React;
const PropTypes = require("prop-types");
const dom = require("react-dom-factories");

class Draggable extends Component {
  static get propTypes() {
    return {
      onMove: PropTypes.func.isRequired,
      onStart: PropTypes.func,
      onStop: PropTypes.func,
      style: PropTypes.object,
      className: PropTypes.string,
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

    // When the target is outside of the document, its tagName is undefined
    if (!ev.target.tagName) {
      return;
    }

    // We pass the whole event because we don't know which properties
    // the callee needs.
    this.props.onMove(ev);
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
      onMouseDown: this.startDragging,
    });
  }
}

module.exports = Draggable;
