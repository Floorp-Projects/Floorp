/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "devtools/client/shared/vendor/react";
import { button } from "devtools/client/shared/vendor/react-dom-factories";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";

import AccessibleImage from "../AccessibleImage";

const classnames = require("resource://devtools/client/shared/classnames.js");

export function debugBtn(
  onClick,
  type,
  className,
  tooltip,
  disabled = false,
  ariaPressed = false
) {
  return React.createElement(
    CommandBarButton,
    {
      className: classnames(type, className),
      disabled: disabled,
      key: type,
      onClick: onClick,
      pressed: ariaPressed,
      title: tooltip,
    },
    React.createElement(AccessibleImage, {
      className: type,
    })
  );
}
const CommandBarButton = props => {
  const { children, className, pressed = false, ...rest } = props;

  return button(
    {
      "aria-pressed": pressed,
      className: classnames("command-bar-button", className),
      ...rest,
    },
    children
  );
};

CommandBarButton.propTypes = {
  children: PropTypes.node.isRequired,
  className: PropTypes.string.isRequired,
  pressed: PropTypes.bool,
};

export default CommandBarButton;
