/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const Draggable = createFactory(
  require("devtools/client/shared/components/splitter/Draggable")
);

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
      initialSize: PropTypes.oneOfType([PropTypes.number, PropTypes.string]),
      // Initial width of controlled panel.
      initialWidth: PropTypes.oneOfType([PropTypes.number, PropTypes.string]),
      // Initial height of controlled panel.
      initialHeight: PropTypes.oneOfType([PropTypes.number, PropTypes.string]),
      // Left/top panel
      startPanel: PropTypes.any,
      // Left/top panel collapse state.
      startPanelCollapsed: PropTypes.bool,
      // Min panel size.
      minSize: PropTypes.oneOfType([PropTypes.number, PropTypes.string]),
      // Max panel size.
      maxSize: PropTypes.oneOfType([PropTypes.number, PropTypes.string]),
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
      // Style object.
      style: PropTypes.object,
      // Call when controlled panel was resized.
      onControlledPanelResized: PropTypes.func,
      // Optional callback when splitbox resize stops
      onResizeEnd: PropTypes.func,
      // Retrieve DOM reference to the start panel element
      onSelectContainerElement: PropTypes.any,
    };
  }

  static get defaultProps() {
    return {
      splitterSize: 5,
      vert: true,
      endPanelControl: false,
    };
  }

  constructor(props) {
    super(props);

    /**
     * The state stores whether or not the end panel should be controlled, the current
     * orientation (vertical or horizontal), the splitter size, and the current size
     * (width/height). All these values can change during the component's life time.
     */
    this.state = {
      // True if the right/bottom panel should be controlled.
      endPanelControl: props.endPanelControl,
      // True if the splitter bar is vertical (default is vertical).
      vert: props.vert,
      // Size of the splitter handle bar.
      splitterSize: props.splitterSize,
      // Width of controlled panel.
      width: props.initialWidth || props.initialSize,
      // Height of controlled panel.
      height: props.initialHeight || props.initialSize,
    };

    this.onStartMove = this.onStartMove.bind(this);
    this.onStopMove = this.onStopMove.bind(this);
    this.onMove = this.onMove.bind(this);
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    const { endPanelControl, splitterSize, vert } = nextProps;

    if (endPanelControl != this.props.endPanelControl) {
      this.setState({ endPanelControl });
    }

    if (splitterSize != this.props.splitterSize) {
      this.setState({ splitterSize });
    }

    if (vert !== this.props.vert) {
      this.setState({ vert });
    }
  }

  shouldComponentUpdate(nextProps, nextState) {
    return (
      nextState.width != this.state.width ||
      nextState.endPanelControl != this.props.endPanelControl ||
      nextState.height != this.state.height ||
      nextState.vert != this.state.vert ||
      nextState.splitterSize != this.state.splitterSize ||
      nextProps.startPanel != this.props.startPanel ||
      nextProps.endPanel != this.props.endPanel ||
      nextProps.minSize != this.props.minSize ||
      nextProps.maxSize != this.props.maxSize
    );
  }

  componentDidUpdate(prevProps, prevState) {
    if (
      this.props.onControlledPanelResized &&
      (prevState.width !== this.state.width ||
        prevState.height !== this.state.height)
    ) {
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
    const doc = this.splitBox.ownerDocument;
    const defaultCursor = doc.documentElement.style.cursor;
    doc.documentElement.style.cursor = this.state.vert
      ? "ew-resize"
      : "ns-resize";

    this.splitBox.classList.add("dragging");

    this.setState({
      defaultCursor,
    });
  }

  onStopMove() {
    const doc = this.splitBox.ownerDocument;
    doc.documentElement.style.cursor = this.state.defaultCursor;

    this.splitBox.classList.remove("dragging");

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
  onMove(x, y) {
    const nodeBounds = this.splitBox.getBoundingClientRect();

    let size;
    let { endPanelControl, vert } = this.state;

    if (vert) {
      // Use the document owning the SplitBox to detect rtl. The global document might be
      // the one bound to the toolbox shared BrowserRequire, which is irrelevant here.
      const doc = this.splitBox.ownerDocument;

      // Switch the control flag in case of RTL. Note that RTL
      // has impact on vertical splitter only.
      if (doc.dir === "rtl") {
        endPanelControl = !endPanelControl;
      }

      size = endPanelControl
        ? nodeBounds.left + nodeBounds.width - x
        : x - nodeBounds.left;

      this.setState({
        width: this.getConstrainedSizeInPx(size, nodeBounds.width),
      });
    } else {
      size = endPanelControl
        ? nodeBounds.top + nodeBounds.height - y
        : y - nodeBounds.top;

      this.setState({
        height: this.getConstrainedSizeInPx(size, nodeBounds.height),
      });
    }
  }

  /**
   * Calculates the constrained size taking into account the minimum width or
   * height passed via this.props.minSize.
   *
   * @param {Number} requestedSize
   *        The requested size
   * @param {Number} splitBoxWidthOrHeight
   *        The width or height of the splitBox
   *
   * @return {Number}
   *         The constrained size
   */
  getConstrainedSizeInPx(requestedSize, splitBoxWidthOrHeight) {
    let minSize = this.props.minSize + "";

    if (minSize.endsWith("%")) {
      minSize = (parseFloat(minSize) / 100) * splitBoxWidthOrHeight;
    } else if (minSize.endsWith("px")) {
      minSize = parseFloat(minSize);
    }
    return Math.max(requestedSize, minSize);
  }

  // Rendering

  // eslint-disable-next-line complexity
  render() {
    const { endPanelControl, splitterSize, vert } = this.state;
    const {
      startPanel,
      startPanelCollapsed,
      endPanel,
      endPanelCollapsed,
      minSize,
      maxSize,
      onSelectContainerElement,
    } = this.props;

    const style = Object.assign(
      {
        // Set the size of the controlled panel (height or width depending on the
        // current state). This can be used to help with styling of dependent
        // panels.
        "--split-box-controlled-panel-size": `${
          vert ? this.state.width : this.state.height
        }`,
      },
      this.props.style
    );

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
        width: endPanelControl ? null : this.state.width,
      };
      rightPanelStyle = {
        maxWidth: endPanelControl ? maxSize : null,
        minWidth: endPanelControl ? minSize : null,
        width: endPanelControl ? this.state.width : null,
      };
    } else {
      leftPanelStyle = {
        maxHeight: endPanelControl ? null : maxSize,
        minHeight: endPanelControl ? null : minSize,
        height: endPanelControl ? null : this.state.height,
      };
      rightPanelStyle = {
        maxHeight: endPanelControl ? maxSize : null,
        minHeight: endPanelControl ? minSize : null,
        height: endPanelControl ? this.state.height : null,
      };
    }

    // Calculate splitter size
    const splitterStyle = {
      flex: "0 0 " + splitterSize + "px",
    };

    return dom.div(
      {
        className: classNames.join(" "),
        ref: div => {
          this.splitBox = div;
        },
        style,
      },
      startPanel && !startPanelCollapsed
        ? dom.div(
            {
              className: endPanelControl ? "uncontrolled" : "controlled",
              style: leftPanelStyle,
              role: "presentation",
              ref: div => {
                this.startPanelContainer = div;
                if (onSelectContainerElement) {
                  onSelectContainerElement(div);
                }
              },
            },
            startPanel
          )
        : null,
      splitterSize > 0
        ? Draggable({
            className: "splitter",
            style: splitterStyle,
            onStart: this.onStartMove,
            onStop: this.onStopMove,
            onMove: this.onMove,
          })
        : null,
      endPanel && !endPanelCollapsed
        ? dom.div(
            {
              className: endPanelControl ? "controlled" : "uncontrolled",
              style: rightPanelStyle,
              role: "presentation",
              ref: div => {
                this.endPanelContainer = div;
              },
            },
            endPanel
          )
        : null
    );
  }
}

module.exports = SplitBox;
