/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { button } = require("devtools/client/shared/vendor/react-dom-factories");

class UIButton extends PureComponent {
  static get propTypes() {
    return {
      children: PropTypes.node,
      className: PropTypes.string,
      disabled: PropTypes.bool,
      onClick: PropTypes.func,
      size: PropTypes.oneOf(["micro"]),
    };
  }

  render() {
    const { className, disabled, onClick, size } = this.props;
    const sizeClass = size ? `ui-button--${size}` : "";

    return button(
      {
        className: `ui-button ${className || ""} ${sizeClass}`,
        onClick,
        disabled,
      },
      this.props.children
    );
  }
}

module.exports = UIButton;
