/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { button, span } = require("devtools/client/shared/vendor/react-dom-factories");

const defaultProps = {
  disabled: false,
  busy: false,
  title: null,
  children: null,
  className: "",
};

/**
 * Button component that handles keyboard in an accessible way. When user
 * uses the mouse to hover/click on the button, there is no focus
 * highlighting. However if the user uses a keyboard to focus on the button,
 * it will have focus highlighting in the form of an outline.
 */
class Button extends Component {
  static get propTypes() {
    return {
      disabled: PropTypes.bool,
      busy: PropTypes.bool,
      title: PropTypes.string,
      children: PropTypes.string,
      className: PropTypes.string,
    };
  }

  static get defaultProps() {
    return defaultProps;
  }

  render() {
    const className = [
      ...this.props.className.split(" "),
      "devtools-button",
    ].join(" ");
    const props = Object.assign({}, this.props, {
      className,
      "aria-busy": this.props.busy.toString(),
      busy: this.props.busy.toString(),
    });

    const classList = ["btn-content"];
    if (this.props.busy) {
      classList.push("devtools-throbber");
    }

    return (button(props, span({
      className: classList.join(" "),
      tabIndex: -1,
    }, this.props.children)));
  }
}

function ToggleButton(props) {
  const {
    active,
    busy,
    disabled,
    label,
    className,
    onClick,
    onKeyDown,
    tooltip,
  } = props;
  const classList = [
    ...className.split(" "),
    "toggle-button",
  ];

  if (active) {
    classList.push("checked");
  }

  if (busy) {
    classList.push("devtools-throbber");
  }

  return button({
    disabled,
    "aria-pressed": active === true,
    "aria-busy": busy,
    className: classList.join(" "),
    onClick,
    onKeyDown,
    title: tooltip,
  }, label);
}

module.exports = {
  Button,
  ToggleButton,
};
