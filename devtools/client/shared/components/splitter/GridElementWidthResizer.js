/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const Draggable = createFactory(
  require("resource://devtools/client/shared/components/splitter/Draggable.js")
);

class GridElementWidthResizer extends Component {
  static get propTypes() {
    return {
      getControlledElementNode: PropTypes.func.isRequired,
      enabled: PropTypes.bool,
      position: PropTypes.string.isRequired,
      className: PropTypes.string,
      onResizeEnd: PropTypes.func,
    };
  }

  constructor(props) {
    super(props);

    this.onStartMove = this.onStartMove.bind(this);
    this.onStopMove = this.onStopMove.bind(this);
    this.onMove = this.onMove.bind(this);
    this.state = {
      dragging: false,
      isRTLElement: false,
      defaultCursor: null,
      defaultWidth: null,
    };
  }

  componentDidUpdate(prevProps) {
    if (prevProps.enabled === true && this.props.enabled === false) {
      this.onStopMove();
      const controlledElementNode = this.props.getControlledElementNode();
      controlledElementNode.style.width = this.state.defaultWidth;
    }
  }

  // Dragging Events

  /**
   * Set 'resizing' cursor on entire document during splitter dragging.
   * This avoids cursor-flickering that happens when the mouse leaves
   * the splitter bar area (happens frequently).
   */
  onStartMove() {
    const controlledElementNode = this.props.getControlledElementNode();
    if (!controlledElementNode) {
      return;
    }

    const doc = controlledElementNode.ownerDocument;
    const defaultCursor = doc.documentElement.style.cursor;
    const defaultWidth = doc.documentElement.style.width;
    doc.documentElement.style.cursor = "ew-resize";
    doc.firstElementChild.classList.add("dragging");

    this.setState({
      dragging: true,
      isRTLElement:
        controlledElementNode.ownerDocument.defaultView.getComputedStyle(
          controlledElementNode
        ).direction === "rtl",
      defaultCursor,
      defaultWidth,
    });
  }

  onStopMove() {
    const controlledElementNode = this.props.getControlledElementNode();
    if (!this.state.dragging || !controlledElementNode) {
      return;
    }
    const doc = controlledElementNode.ownerDocument;
    doc.documentElement.style.cursor = this.state.defaultCursor;
    doc.firstElementChild.classList.remove("dragging");

    this.setState({
      dragging: false,
    });

    if (this.props.onResizeEnd) {
      const { width } = controlledElementNode.getBoundingClientRect();
      this.props.onResizeEnd(width);
    }
  }

  /**
   * Adjust size of the controlled panel.
   */
  onMove(x) {
    const controlledElementNode = this.props.getControlledElementNode();
    if (!this.state.dragging || !controlledElementNode) {
      return;
    }
    const nodeBounds = controlledElementNode.getBoundingClientRect();
    const { isRTLElement } = this.state;
    const { position } = this.props;

    const size =
      (isRTLElement && position === "end") ||
      (!isRTLElement && position === "start")
        ? nodeBounds.width + (nodeBounds.left - x)
        : x - nodeBounds.left;

    controlledElementNode.style.width = `${size}px`;
  }

  render() {
    if (!this.props.enabled) {
      return null;
    }

    const classNames = ["grid-element-width-resizer", this.props.position];
    if (this.props.className) {
      classNames.push(this.props.className);
    }

    return Draggable({
      className: classNames.join(" "),
      onStart: this.onStartMove,
      onStop: this.onStopMove,
      onMove: this.onMove,
    });
  }
}

module.exports = GridElementWidthResizer;
