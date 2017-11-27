/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const Draggable = createFactory(require("devtools/client/shared/components/splitter/Draggable"));

/**
 * This component represents a Splitter. The splitter supports vertical
 * as well as horizontal mode.
 */
class SplitBox extends Component {
  static get propTypes() {
    return {
      // Custom class name. You can use more names separated by a space.
      className: PropTypes.string,
      // Initial size of controlled panel.
      initialSize: PropTypes.string,
      // Initial width of controlled panel.
      initialWidth: PropTypes.string,
      // Initial height of controlled panel.
      initialHeight: PropTypes.string,
      // Left/top panel
      startPanel: PropTypes.any,
      // Min panel size.
      minSize: PropTypes.string,
      // Max panel size.
      maxSize: PropTypes.string,
      // Right/bottom panel
      endPanel: PropTypes.any,
      // True if the right/bottom panel should be controlled.
      endPanelControl: PropTypes.bool,
      // Size of the splitter handle bar.
      splitterSize: PropTypes.string,
      // True if the splitter bar is vertical (default is vertical).
      vert: PropTypes.bool,
      // Style object.
      style: PropTypes.object,
      // Call when controlled panel was resized.
      onControlledPanelResized: PropTypes.func,
    };
  }

  static get defaultProps() {
    return {
      splitterSize: 5,
      vert: true,
      endPanelControl: false
    };
  }

  constructor(props) {
    super(props);

    /**
     * The state stores the current orientation (vertical or horizontal)
     * and the current size (width/height). All these values can change
     * during the component's life time.
     */
    this.state = {
      vert: props.vert,
      splitterSize: props.splitterSize,
      width: props.initialWidth || props.initialSize,
      height: props.initialHeight || props.initialSize
    };

    this.onStartMove = this.onStartMove.bind(this);
    this.onStopMove = this.onStopMove.bind(this);
    this.onMove = this.onMove.bind(this);
  }

  componentWillReceiveProps(nextProps) {
    let { splitterSize, vert } = nextProps;

    if (splitterSize != this.props.splitterSize) {
      this.setState({ splitterSize });
    }

    if (vert !== this.props.vert) {
      this.setState({ vert });
    }
  }

  shouldComponentUpdate(nextProps, nextState) {
    return nextState.width != this.state.width ||
      nextState.height != this.state.height ||
      nextState.vert != this.state.vert ||
      nextState.splitterSize != this.state.splitterSize ||
      nextProps.startPanel != this.props.startPanel ||
      nextProps.endPanel != this.props.endPanel ||
      nextProps.endPanelControl != this.props.endPanelControl ||
      nextProps.minSize != this.props.minSize ||
      nextProps.maxSize != this.props.maxSize;
  }

  componentDidUpdate(prevProps, prevState) {
    if (this.props.onControlledPanelResized && (prevState.width !== this.state.width ||
                                                prevState.height !== this.state.height)) {
      this.props.onControlledPanelResized(this.state.width, this.state.height);
    }
  }

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
  }

  onStopMove() {
    const splitBox = ReactDOM.findDOMNode(this);
    const doc = splitBox.ownerDocument;
    doc.documentElement.style.cursor = this.state.defaultCursor;

    splitBox.classList.remove("dragging");
  }

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
  }

  // Rendering

  render() {
    const { splitterSize, vert } = this.state;
    const { startPanel, endPanel, endPanelControl, minSize, maxSize } = this.props;

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
            style: leftPanelStyle,
            ref: div => this.startPanelContainer = div},
            startPanel
          ) : null,
        splitterSize > 0 ?
          Draggable({
            className: "splitter",
            style: splitterStyle,
            onStart: this.onStartMove,
            onStop: this.onStopMove,
            onMove: this.onMove
          }) : null,
        endPanel ?
          dom.div({
            className: endPanelControl ? "controlled" : "uncontrolled",
            style: rightPanelStyle,
            ref: div => this.endPanelContainer = div},
            endPanel
          ) : null
      )
    );
  }
}

module.exports = SplitBox;
