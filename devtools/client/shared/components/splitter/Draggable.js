/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createRef,
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

class Draggable extends Component {
  static get propTypes() {
    return {
      onMove: PropTypes.func.isRequired,
      onDoubleClick: PropTypes.func,
      onStart: PropTypes.func,
      onStop: PropTypes.func,
      style: PropTypes.object,
      title: PropTypes.string,
      className: PropTypes.string,
    };
  }

  constructor(props) {
    super(props);

    this.draggableEl = createRef();

    this.startDragging = this.startDragging.bind(this);
    this.stopDragging = this.stopDragging.bind(this);
    this.onDoubleClick = this.onDoubleClick.bind(this);
    this.onMove = this.onMove.bind(this);

    this.mouseX = 0;
    this.mouseY = 0;
  }
  startDragging(ev) {
    const xDiff = Math.abs(this.mouseX - ev.clientX);
    const yDiff = Math.abs(this.mouseY - ev.clientY);

    // This allows for double-click.
    if (this.props.onDoubleClick && xDiff + yDiff <= 1) {
      return;
    }
    this.mouseX = ev.clientX;
    this.mouseY = ev.clientY;

    if (this.isDragging) {
      return;
    }
    this.isDragging = true;
    ev.preventDefault();

    this.draggableEl.current.addEventListener("mousemove", this.onMove);
    this.draggableEl.current.setPointerCapture(ev.pointerId);

    this.props.onStart && this.props.onStart();
  }

  onDoubleClick() {
    if (this.props.onDoubleClick) {
      this.props.onDoubleClick();
    }
  }

  onMove(ev) {
    if (!this.isDragging) {
      return;
    }

    ev.preventDefault();
    // Use viewport coordinates so, moving mouse over iframes
    // doesn't mangle (relative) coordinates.
    this.props.onMove(ev.clientX, ev.clientY);
  }

  stopDragging(ev) {
    if (!this.isDragging) {
      return;
    }
    this.isDragging = false;
    ev.preventDefault();

    this.draggableEl.current.removeEventListener("mousemove", this.onMove);
    this.draggableEl.current.releasePointerCapture(ev.pointerId);
    this.props.onStop && this.props.onStop();
  }

  render() {
    return dom.div({
      ref: this.draggableEl,
      role: "presentation",
      style: this.props.style,
      title: this.props.title,
      className: this.props.className,
      onMouseDown: this.startDragging,
      onMouseUp: this.stopDragging,
      onDoubleClick: this.onDoubleClick,
    });
  }
}

module.exports = Draggable;
