/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import classnames from "classnames";
import React from "react";

import AccessibleImage from "../AccessibleImage";

import "./styles/CommandBarButton.css";

export function debugBtn(
  onClick,
  type,
  className,
  tooltip,
  disabled = false,
  ariaPressed = false
) {
  return (
    <CommandBarButton
      className={classnames(type, className)}
      disabled={disabled}
      key={type}
      onClick={onClick}
      pressed={ariaPressed}
      title={tooltip}
    >
      <AccessibleImage className={type} />
    </CommandBarButton>
  );
}

const CommandBarButton = props => {
  const { children, className, pressed = false, ...rest } = props;

  return (
    <button
      aria-pressed={pressed}
      className={classnames("command-bar-button", className)}
      {...rest}
    >
      {children}
    </button>
  );
};

export default CommandBarButton;
