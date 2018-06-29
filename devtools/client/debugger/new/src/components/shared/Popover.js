"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _BracketArrow = require("./BracketArrow");

var _BracketArrow2 = _interopRequireDefault(_BracketArrow);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class Popover extends _react.Component {
  constructor(props) {
    super(props);

    this.calculateTopForRightOrientation = (target, editor, popover) => {
      if (popover.height < editor.height) {
        const rightOrientationTop = target.top - popover.height / 2;

        if (rightOrientationTop < editor.top) {
          return editor.top;
        }

        const rightOrientationBottom = rightOrientationTop + popover.height;

        if (rightOrientationBottom > editor.bottom) {
          return editor.bottom - popover.height;
        }

        return rightOrientationTop;
      }

      return 0;
    };

    this.calculateTop = (target, editor, popover, orientation) => {
      if (orientation === "down") {
        return target.bottom;
      }

      if (orientation === "up") {
        return target.top - popover.height;
      }

      return this.calculateTopForRightOrientation(target, editor, popover);
    };

    this.state = {
      left: 0,
      top: 0,
      targetMid: {
        x: 0,
        y: 0
      },
      orientation: "up"
    };
  }

  componentDidMount() {
    const {
      type
    } = this.props;
    const {
      left,
      top,
      orientation,
      targetMid
    } = type == "popover" ? this.getPopoverCoords() : this.getTooltipCoords(); // eslint-disable-next-line react/no-did-mount-set-state

    this.setState({
      left,
      top,
      orientation,
      targetMid
    });
  }

  calculateLeft(target, editor, popover, orientation) {
    const estimatedLeft = target.left;
    const estimatedRight = estimatedLeft + popover.width;
    const isOverflowingRight = estimatedRight > editor.right;

    if (orientation === "right") {
      return target.left + target.width + 10;
    }

    if (isOverflowingRight) {
      const adjustedLeft = editor.right - popover.width - 8;
      return adjustedLeft;
    }

    return estimatedLeft;
  }

  calculateOrientation(target, editor, popover) {
    const estimatedBottom = target.bottom + popover.height;

    if (editor.bottom > estimatedBottom) {
      return "down";
    }

    const upOrientationTop = target.top - popover.height;

    if (upOrientationTop > editor.top) {
      return "up";
    }

    return "right";
  }

  getPopoverCoords() {
    if (this.$popover && this.props.editorRef) {
      const popover = this.$popover;
      const editor = this.props.editorRef;
      const popoverRect = popover.getBoundingClientRect();
      const editorRect = editor.getBoundingClientRect();
      const targetRect = this.props.targetPosition;
      const orientation = this.calculateOrientation(targetRect, editorRect, popoverRect);
      const top = this.calculateTop(targetRect, editorRect, popoverRect, orientation);
      const popoverLeft = this.calculateLeft(targetRect, editorRect, popoverRect, orientation);
      let targetMid;

      if (orientation === "right") {
        targetMid = {
          x: -14,
          y: targetRect.top - top - 2
        };
      } else {
        targetMid = {
          x: targetRect.left - popoverLeft + targetRect.width / 2 - 8,
          y: 0
        };
      }

      return {
        left: popoverLeft,
        top,
        orientation,
        targetMid
      };
    }

    return {
      left: 0,
      top: 0,
      orientation: "down",
      targetMid: {
        x: 0,
        y: 0
      }
    };
  }

  getTooltipCoords() {
    if (this.$tooltip && this.props.editorRef) {
      const tooltip = this.$tooltip;
      const editor = this.props.editorRef;
      const tooltipRect = tooltip.getBoundingClientRect();
      const editorRect = editor.getBoundingClientRect();
      const targetRect = this.props.targetPosition;
      const left = this.calculateLeft(targetRect, editorRect, tooltipRect);
      const top = targetRect.top - tooltipRect.height;
      return {
        left,
        top,
        orientation: "up",
        targetMid: {
          x: 0,
          y: 0
        }
      };
    }

    return {
      left: 0,
      top: 0,
      orientation: "up",
      targetMid: {
        x: 0,
        y: 0
      }
    };
  }

  getChildren() {
    const {
      children
    } = this.props;
    const {
      orientation
    } = this.state;

    const gap = _react2.default.createElement("div", {
      className: "gap",
      key: "gap"
    });

    return orientation === "up" ? [children, gap] : [gap, children];
  }

  getPopoverArrow(orientation, left, top) {
    const arrowProps = {};

    if (orientation === "up") {
      Object.assign(arrowProps, {
        orientation: "down",
        bottom: 5,
        left
      });
    } else if (orientation === "down") {
      Object.assign(arrowProps, {
        orientation: "up",
        top: -7,
        left
      });
    } else {
      Object.assign(arrowProps, {
        orientation: "left",
        top,
        left: -14
      });
    }

    return _react2.default.createElement(_BracketArrow2.default, arrowProps);
  }

  renderPopover() {
    const {
      top,
      left,
      orientation,
      targetMid
    } = this.state;
    const arrow = this.getPopoverArrow(orientation, targetMid.x, targetMid.y);
    return _react2.default.createElement("div", {
      className: (0, _classnames2.default)("popover", `orientation-${orientation}`, {
        up: orientation === "up"
      }),
      onMouseLeave: this.props.onMouseLeave,
      style: {
        top,
        left
      },
      ref: c => this.$popover = c
    }, arrow, this.getChildren());
  }

  renderTooltip() {
    const {
      onMouseLeave
    } = this.props;
    const {
      top,
      left
    } = this.state;
    return _react2.default.createElement("div", {
      className: "tooltip",
      onMouseLeave: onMouseLeave,
      style: {
        top,
        left
      },
      ref: c => this.$tooltip = c
    }, this.getChildren());
  }

  render() {
    const {
      type
    } = this.props;

    if (type === "tooltip") {
      return this.renderTooltip();
    }

    return this.renderPopover();
  }

}

Popover.defaultProps = {
  onMouseLeave: () => {},
  type: "popover"
};
exports.default = Popover;