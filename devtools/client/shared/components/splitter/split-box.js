/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const React = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const Draggable = React.createFactory(require("devtools/client/shared/components/splitter/draggable"));
const { DOM: dom, PropTypes } = React;

/**
 * This component represents a Splitter. The splitter supports vertical
 * as well as horizontal mode.
 */
const SplitBox = React.createClass({
  displayName: "SplitBox",

  propTypes: {
    // Custom class name. You can use more names separated by a space.
    className: PropTypes.string,
    // Initial size of controlled panel.
    initialSize: PropTypes.number,
    // Initial width of controlled panel.
    initialWidth: PropTypes.number,
    // Initial height of controlled panel.
    initialHeight: PropTypes.number,
    // Left/top panel
    startPanel: PropTypes.any,
    // Min panel size.
    minSize: PropTypes.number,
    // Max panel size.
    maxSize: PropTypes.number,
    // Right/bottom panel
    endPanel: PropTypes.any,
    // True if the right/bottom panel should be controlled.
    endPanelControl: PropTypes.bool,
    // Size of the splitter handle bar.
    splitterSize: PropTypes.number,
    // True if the splitter bar is vertical (default is vertical).
    vert: PropTypes.bool,
    // Style object.
    style: PropTypes.object,
  },

  getDefaultProps() {
    return {
      splitterSize: 5,
      vert: true,
      endPanelControl: false
    };
  },

  /**
   * The state stores the current orientation (vertical or horizontal)
   * and the current size (width/height). All these values can change
   * during the component's life time.
   */
  getInitialState() {
    return {
      vert: this.props.vert,
      width: this.props.initialWidth || this.props.initialSize,
      height: this.props.initialHeight || this.props.initialSize
    };
  },

  // Dragging Events

  /**
   * Set 'resizing' cursor on entire document during splitter dragging.
   * This avoids cursor-flickering that happens when the mouse leaves
   * the splitter bar area (happens frequently).
   */
  onStartMove() {
    const splitBox = ReactDOM.findDOMNode(this);
    const doc = splitBox.ownerDocument;
    let defaultCursor = doc.documentElement.style.cursor;
    doc.documentElement.style.cursor = (this.state.vert ? "ew-resize" : "ns-resize");

    splitBox.classList.add("dragging");

    this.setState({
      defaultCursor: defaultCursor
    });
  },

  onStopMove() {
    const splitBox = ReactDOM.findDOMNode(this);
    const doc = splitBox.ownerDocument;
    doc.documentElement.style.cursor = this.state.defaultCursor;

    splitBox.classList.remove("dragging");
  },

  /**
   * Adjust size of the controlled panel. Depending on the current
   * orientation we either remember the width or height of
   * the splitter box.
   */
  onMove(x, y) {
    const node = ReactDOM.findDOMNode(this);
    const doc = node.ownerDocument;
    const win = doc.defaultView;

    let size;
    let { endPanelControl } = this.props;

    if (this.state.vert) {
      // Switch the control flag in case of RTL. Note that RTL
      // has impact on vertical splitter only.
      let dir = win.getComputedStyle(doc.documentElement).direction;
      if (dir == "rtl") {
        endPanelControl = !endPanelControl;
      }

      size = endPanelControl ?
        (node.offsetLeft + node.offsetWidth) - x :
        x - node.offsetLeft;

      this.setState({
        width: size
      });
    } else {
      size = endPanelControl ?
        (node.offsetTop + node.offsetHeight) - y :
        y - node.offsetTop;

      this.setState({
        height: size
      });
    }
  },

  // Rendering

  render() {
    const vert = this.state.vert;
    const { startPanel, endPanel, endPanelControl, minSize,
      maxSize, splitterSize } = this.props;

    let style = Object.assign({}, this.props.style);

    // Calculate class names list.
    let classNames = ["split-box"];
    classNames.push(vert ? "vert" : "horz");
    if (this.props.className) {
      classNames = classNames.concat(this.props.className.split(" "));
    }

    let leftPanelStyle;
    let rightPanelStyle;

    // Set proper size for panels depending on the current state.
    if (vert) {
      leftPanelStyle = {
        maxWidth: endPanelControl ? null : maxSize,
        minWidth: endPanelControl ? null : minSize,
        width: endPanelControl ? null : this.state.width
      };
      rightPanelStyle = {
        maxWidth: endPanelControl ? maxSize : null,
        minWidth: endPanelControl ? minSize : null,
        width: endPanelControl ? this.state.width : null
      };
    } else {
      leftPanelStyle = {
        maxHeight: endPanelControl ? null : maxSize,
        minHeight: endPanelControl ? null : minSize,
        height: endPanelControl ? null : this.state.height
      };
      rightPanelStyle = {
        maxHeight: endPanelControl ? maxSize : null,
        minHeight: endPanelControl ? minSize : null,
        height: endPanelControl ? this.state.height : null
      };
    }

    // Calculate splitter size
    let splitterStyle = {
      flex: "0 0 " + splitterSize + "px"
    };

    return (
      dom.div({
        className: classNames.join(" "),
        style: style },
        startPanel ?
          dom.div({
            className: endPanelControl ? "uncontrolled" : "controlled",
            style: leftPanelStyle},
            startPanel
          ) : null,
        Draggable({
          className: "splitter",
          style: splitterStyle,
          onStart: this.onStartMove,
          onStop: this.onStopMove,
          onMove: this.onMove
        }),
        endPanel ?
          dom.div({
            className: endPanelControl ? "controlled" : "uncontrolled",
            style: rightPanelStyle},
            endPanel
          ) : null
      )
    );
  }
});

module.exports = SplitBox;
