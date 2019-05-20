/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { PureComponent } from "react";
import classnames from "classnames";
import AccessibleImage from "../AccessibleImage";
import { CommandBarButton } from "./";
import "./styles/PaneToggleButton.css";

type Position = "start" | "end";

type Props = {
  collapsed: boolean,
  handleClick: (Position, boolean) => void,
  horizontal: boolean,
  position: Position,
};

class PaneToggleButton extends PureComponent<Props> {
  static defaultProps = {
    horizontal: false,
    position: "start",
  };

  label(position: Position, collapsed: boolean) {
    switch (position) {
      case "start":
        return L10N.getStr(collapsed ? "expandSources" : "collapseSources");
      case "end":
        return L10N.getStr(
          collapsed ? "expandBreakpoints" : "collapseBreakpoints"
        );
    }
  }

  render() {
    const { position, collapsed, horizontal, handleClick } = this.props;

    return (
      <CommandBarButton
        className={classnames("toggle-button", position, {
          collapsed,
          vertical: !horizontal,
        })}
        onClick={() => handleClick(position, !collapsed)}
        title={this.label(position, collapsed)}
      >
        <AccessibleImage
          className={collapsed ? "pane-expand" : "pane-collapse"}
        />
      </CommandBarButton>
    );
  }
}

export default PaneToggleButton;
