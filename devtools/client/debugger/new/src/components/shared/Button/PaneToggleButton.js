/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { PureComponent } from "react";
import classnames from "classnames";
import AccessibleImage from "../AccessibleImage";
import { CommandBarButton } from "./";
import "./styles/PaneToggleButton.css";

type Props = {
  collapsed: boolean,
  handleClick: (string, boolean) => void,
  horizontal: boolean,
  position: string
};

class PaneToggleButton extends PureComponent<Props> {
  static defaultProps = {
    horizontal: false
  };

  render() {
    const { position, collapsed, horizontal, handleClick } = this.props;
    const title = collapsed
      ? L10N.getStr("expandPanes")
      : L10N.getStr("collapsePanes");

    return (
      <CommandBarButton
        className={classnames("toggle-button", position, {
          collapsed,
          vertical: !horizontal
        })}
        onClick={() => handleClick(position, !collapsed)}
        title={title}
      >
        <AccessibleImage
          className={collapsed ? "pane-expand" : "pane-collapse"}
        />
      </CommandBarButton>
    );
  }
}

export default PaneToggleButton;
