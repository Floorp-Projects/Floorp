/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const React = require("react");
const ReactDOM = require("react-dom");
const Draggable = React.createFactory(require("./Draggable"));
const { Component } = React;
const PropTypes = require("prop-types");
const dom = require("react-dom-factories");

require("./SplitBox.css");

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
      initialSize: PropTypes.any,
      // Optional initial width of controlled panel.
      initialWidth: PropTypes.number,
      // Optional initial height of controlled panel.
      initialHeight: PropTypes.number,
      // Left/top panel
      startPanel: PropTypes.any,
      // Left/top panel collapse state.
      startPanelCollapsed: PropTypes.bool,
      // Min panel size.
      minSize: PropTypes.any,
      // Max panel size.
      maxSize: PropTypes.any,
      // Right/bottom panel
      endPanel: PropTypes.any,
      // Right/bottom panel collapse state.
      endPanelCollapsed: PropTypes.bool,
      // True if the right/bottom panel should be controlled.
      endPanelControl: PropTypes.bool,
      // Size of the splitter handle bar.
      splitterSize: PropTypes.number,
      // True if the splitter bar is vertical (default is vertical).
      vert: PropTypes.bool,
      // Optional style properties passed into the splitbox
      style: PropTypes.object,
      // Optional callback when splitbox resize stops
      onResizeEnd: PropTypes.func,
    };
  }

  static get defaultProps() {
    return {
      splitterSize: 5,
      vert: true,
      endPanelControl: false,
      endPanelCollapsed: false,
      startPanelCollapsed: false,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      vert: props.vert,
      // We use integers for these properties
      width: parseInt(props.initialWidth || props.initialSize, 10),
      height: parseInt(props.initialHeight || props.initialSize, 10),
    };

    this.onStartMove = this.onStartMove.bind(this);
    this.onStopMove = this.onStopMove.bind(this);
    this.onMove = this.onMove.bind(this);
    this.preparePanelStyles = this.preparePanelStyles.bind(this);
  }

  componentWillReceiveProps(nextProps) {
    if (this.props.vert !== nextProps.vert) {
      this.setState({ vert: nextProps.vert });
    }
    if (
      this.props.initialSize !== nextProps.initialSize ||
      this.props.initialWidth !== nextProps.initialWidth ||
      this.props.initialHeight !== nextProps.initialHeight
    ) {
      this.setState({
        width: parseInt(nextProps.initialWidth || nextProps.initialSize, 10),
        height: parseInt(nextProps.initialHeight || nextProps.initialSize, 10),
      });
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
    const defaultCursor = doc.documentElement.style.cursor;
    doc.documentElement.style.cursor = this.state.vert
      ? "ew-resize"
      : "ns-resize";

    splitBox.classList.add("dragging");
    document.dispatchEvent(new CustomEvent("drag:start"));

    this.setState({
      defaultCursor: defaultCursor,
    });
  }

  onStopMove() {
    const splitBox = ReactDOM.findDOMNode(this);
    const doc = splitBox.ownerDocument;
    doc.documentElement.style.cursor = this.state.defaultCursor;

    splitBox.classList.remove("dragging");
    document.dispatchEvent(new CustomEvent("drag:end"));

    if (this.props.onResizeEnd) {
      this.props.onResizeEnd(
        this.state.vert ? this.state.width : this.state.height
      );
    }
  }

  /**
   * Adjust size of the controlled panel. Depending on the current
   * orientation we either remember the width or height of
   * the splitter box.
   */
  onMove({ movementX, movementY }) {
    const node = ReactDOM.findDOMNode(this);
    const doc = node.ownerDocument;

    if (this.props.endPanelControl) {
      // For the end panel we need to increase the width/height when the
      // movement is towards the left/top.
      movementX = -movementX;
      movementY = -movementY;
    }

    if (this.state.vert) {
      const isRtl = doc.dir === "rtl";
      if (isRtl) {
        // In RTL we need to reverse the movement again -- but only for vertical
        // splitters
        movementX = -movementX;
      }

      this.setState((state, props) => ({
        width: state.width + movementX,
      }));
    } else {
      this.setState((state, props) => ({
        height: state.height + movementY,
      }));
    }
  }

  // Rendering
  preparePanelStyles() {
    const vert = this.state.vert;
    const {
      minSize,
      maxSize,
      startPanelCollapsed,
      endPanelControl,
      endPanelCollapsed,
    } = this.props;
    let leftPanelStyle, rightPanelStyle;

    // Set proper size for panels depending on the current state.
    if (vert) {
      const startWidth = endPanelControl ? null : this.state.width,
        endWidth = endPanelControl ? this.state.width : null;

      leftPanelStyle = {
        maxWidth: endPanelControl ? null : maxSize,
        minWidth: endPanelControl ? null : minSize,
        width: startPanelCollapsed ? 0 : startWidth,
      };
      rightPanelStyle = {
        maxWidth: endPanelControl ? maxSize : null,
        minWidth: endPanelControl ? minSize : null,
        width: endPanelCollapsed ? 0 : endWidth,
      };
    } else {
      const startHeight = endPanelControl ? null : this.state.height,
        endHeight = endPanelControl ? this.state.height : null;

      leftPanelStyle = {
        maxHeight: endPanelControl ? null : maxSize,
        minHeight: endPanelControl ? null : minSize,
        height: endPanelCollapsed ? maxSize : startHeight,
      };
      rightPanelStyle = {
        maxHeight: endPanelControl ? maxSize : null,
        minHeight: endPanelControl ? minSize : null,
        height: startPanelCollapsed ? maxSize : endHeight,
      };
    }

    return { leftPanelStyle, rightPanelStyle };
  }

  render() {
    const vert = this.state.vert;
    const {
      startPanelCollapsed,
      startPanel,
      endPanel,
      endPanelControl,
      splitterSize,
      endPanelCollapsed,
    } = this.props;

    const style = Object.assign({}, this.props.style);

    // Calculate class names list.
    let classNames = ["split-box"];
    classNames.push(vert ? "vert" : "horz");
    if (this.props.className) {
      classNames = classNames.concat(this.props.className.split(" "));
    }

    const { leftPanelStyle, rightPanelStyle } = this.preparePanelStyles();

    // Calculate splitter size
    const splitterStyle = {
      flex: `0 0 ${splitterSize}px`,
    };

    return dom.div(
      {
        className: classNames.join(" "),
        style: style,
      },
      !startPanelCollapsed
        ? dom.div(
            {
              className: endPanelControl ? "uncontrolled" : "controlled",
              style: leftPanelStyle,
            },
            startPanel
          )
        : null,
      Draggable({
        className: "splitter",
        style: splitterStyle,
        onStart: this.onStartMove,
        onStop: this.onStopMove,
        onMove: this.onMove,
      }),
      !endPanelCollapsed
        ? dom.div(
            {
              className: endPanelControl ? "controlled" : "uncontrolled",
              style: rightPanelStyle,
            },
            endPanel
          )
        : null
    );
  }
}

module.exports = SplitBox;
