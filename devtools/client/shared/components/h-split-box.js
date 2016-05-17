/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
"use strict";

// A box with a start and a end pane, separated by a dragable splitter that
// allows the user to resize the relative widths of the panes.
//
//     +-----------------------+---------------------+
//     |                       |                     |
//     |                       |                     |
//     |                       S                     |
//     |      Start Pane       p     End Pane        |
//     |                       l                     |
//     |                       i                     |
//     |                       t                     |
//     |                       t                     |
//     |                       e                     |
//     |                       r                     |
//     |                       |                     |
//     |                       |                     |
//     +-----------------------+---------------------+

const {
  DOM: dom,
  createClass,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { assert } = require("devtools/shared/DevToolsUtils");

module.exports = createClass({
  displayName: "HSplitBox",

  propTypes: {
    // The contents of the start pane.
    start: PropTypes.any.isRequired,

    // The contents of the end pane.
    end: PropTypes.any.isRequired,

    // The relative width of the start pane, expressed as a number between 0 and
    // 1. The relative width of the end pane is 1 - startWidth. For example,
    // with startWidth = .5, both panes are of equal width; with startWidth =
    // .25, the start panel will take up 1/4 width and the end panel will take
    // up 3/4 width.
    startWidth: PropTypes.number,

    // A minimum css width value for the start and end panes.
    minStartWidth: PropTypes.any,
    minEndWidth: PropTypes.any,

    // A callback fired when the user drags the splitter to resize the relative
    // pane widths. The function is passed the startWidth value that would put
    // the splitter underneath the users mouse.
    onResize: PropTypes.func.isRequired,
  },

  getDefaultProps() {
    return {
      startWidth: 0.5,
      minStartWidth: "20px",
      minEndWidth: "20px",
    };
  },

  getInitialState() {
    return {
      mouseDown: false
    };
  },

  componentDidMount() {
    document.defaultView.top.addEventListener("mouseup", this._onMouseUp,
                                              false);
    document.defaultView.top.addEventListener("mousemove", this._onMouseMove,
                                              false);
  },

  componentWillUnmount() {
    document.defaultView.top.removeEventListener("mouseup", this._onMouseUp,
                                                 false);
    document.defaultView.top.removeEventListener("mousemove", this._onMouseMove,
                                                 false);
  },

  _onMouseDown(event) {
    if (event.button !== 0) {
      return;
    }

    this.setState({ mouseDown: true });
    event.preventDefault();
  },

  _onMouseUp(event) {
    if (event.button !== 0 || !this.state.mouseDown) {
      return;
    }

    this.setState({ mouseDown: false });
    event.preventDefault();
  },

  _onMouseMove(event) {
    if (!this.state.mouseDown) {
      return;
    }

    const rect = this.refs.box.getBoundingClientRect();
    const { left, right } = rect;
    const width = right - left;
    const relative = event.clientX - left;
    this.props.onResize(relative / width);

    event.preventDefault();
  },

  render() {
    /* eslint-disable no-shadow */
    const { start, end, startWidth, minStartWidth, minEndWidth } = this.props;
    assert(startWidth => 0 && startWidth <= 1,
           "0 <= this.props.startWidth <= 1");
    /* eslint-enable */
    return dom.div(
      {
        className: "h-split-box",
        ref: "box",
      },

      dom.div(
        {
          className: "h-split-box-pane",
          style: { flex: startWidth, minWidth: minStartWidth },
        },
        start
      ),

      dom.div({
        className: "devtools-side-splitter",
        onMouseDown: this._onMouseDown,
      }),

      dom.div(
        {
          className: "h-split-box-pane",
          style: { flex: 1 - startWidth, minWidth: minEndWidth },
        },
        end
      )
    );
  }
});
