/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import classnames from "classnames";
import React from "react";

import AccessibleImage from "../AccessibleImage";

import "./styles/CommandBarButton.css";

type Props = {
  children: React$Element<any>,
  className: string,
  pressed?: boolean,
};

export function debugBtn(
  onClick: ?Function,
  type: string,
  className: string,
  tooltip: string,
  disabled: boolean = false,
  ariaPressed: boolean = false
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

const CommandBarButton = (props: Props) => {
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
