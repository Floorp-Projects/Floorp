/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "react";
import PropTypes from "prop-types";
import AccessibleImage from "../AccessibleImage";
import { CommandBarButton } from "./";

const classnames = require("devtools/client/shared/classnames.js");

import "./styles/PaneToggleButton.css";

class PaneToggleButton extends PureComponent {
  static defaultProps = {
    horizontal: false,
    position: "start",
  };

  static get propTypes() {
    return {
      collapsed: PropTypes.bool.isRequired,
      handleClick: PropTypes.func.isRequired,
      horizontal: PropTypes.bool.isRequired,
      position: PropTypes.oneOf(["start", "end"]).isRequired,
    };
  }

  label(position, collapsed) {
    switch (position) {
      case "start":
        return L10N.getStr(collapsed ? "expandSources" : "collapseSources");
      case "end":
        return L10N.getStr(
          collapsed ? "expandBreakpoints" : "collapseBreakpoints"
        );
    }
    return null;
  }

  render() {
    const { position, collapsed, horizontal, handleClick } = this.props;
    return React.createElement(
      CommandBarButton,
      {
        className: classnames("toggle-button", position, {
          collapsed,
          vertical: !horizontal,
        }),
        onClick: () => handleClick(position, !collapsed),
        title: this.label(position, collapsed),
      },
      React.createElement(AccessibleImage, {
        className: collapsed ? "pane-expand" : "pane-collapse",
      })
    );
  }
}

export default PaneToggleButton;
