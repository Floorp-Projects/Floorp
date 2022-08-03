/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

// Shortcuts
const { button } = dom;

/**
 * Sidebar toggle button. This button is used to exapand
 * and collapse Sidebar.
 */
class SidebarToggle extends Component {
  static get propTypes() {
    return {
      // Set to true if collapsed.
      collapsed: PropTypes.bool.isRequired,
      // Tooltip text used when the button indicates expanded state.
      collapsePaneTitle: PropTypes.string.isRequired,
      // Tooltip text used when the button indicates collapsed state.
      expandPaneTitle: PropTypes.string.isRequired,
      // Click callback
      onClick: PropTypes.func.isRequired,
      // align toggle button to right
      alignRight: PropTypes.bool,
      // if set to true toggle-button rotate 90
      canVerticalSplit: PropTypes.bool,
    };
  }

  static get defaultProps() {
    return {
      alignRight: false,
      canVerticalSplit: true,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      collapsed: props.collapsed,
    };

    this.onClick = this.onClick.bind(this);
  }

  // Events

  onClick(event) {
    event.stopPropagation();
    this.setState({ collapsed: !this.state.collapsed });
    this.props.onClick(event);
  }

  // Rendering

  render() {
    const title = this.state.collapsed
      ? this.props.expandPaneTitle
      : this.props.collapsePaneTitle;

    const classNames = ["devtools-button", "sidebar-toggle"];
    if (this.state.collapsed) {
      classNames.push("pane-collapsed");
    }
    if (this.props.alignRight) {
      classNames.push("alignRight");
    }
    if (!this.props.canVerticalSplit) {
      classNames.push("disableVerticalBehaviour");
    }

    return button({
      className: classNames.join(" "),
      title,
      onClick: this.onClick,
    });
  }
}

module.exports = SidebarToggle;
