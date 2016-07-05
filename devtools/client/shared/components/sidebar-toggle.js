/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM, createClass, PropTypes } = require("devtools/client/shared/vendor/react");

// Shortcuts
const { button } = DOM;

/**
 * Sidebar toggle button. This button is used to exapand
 * and collapse Sidebar.
 */
var SidebarToggle = createClass({
  displayName: "SidebarToggle",

  propTypes: {
    // Set to true if collapsed.
    collapsed: PropTypes.bool.isRequired,
    // Tooltip text used when the button indicates expanded state.
    collapsePaneTitle: PropTypes.string.isRequired,
    // Tooltip text used when the button indicates collapsed state.
    expandPaneTitle: PropTypes.string.isRequired,
    // Click callback
    onClick: PropTypes.func.isRequired,
  },

  getInitialState: function () {
    return {
      collapsed: this.props.collapsed,
    };
  },

  // Events

  onClick: function (event) {
    this.props.onClick(event);
  },

  // Rendering

  render: function () {
    let title = this.state.collapsed ?
      this.props.expandPaneTitle :
      this.props.collapsePaneTitle;

    let classNames = ["devtools-button", "sidebar-toggle"];
    if (this.state.collapsed) {
      classNames.push("pane-collapsed");
    }

    return (
      button({
        className: classNames.join(" "),
        title: title,
        onClick: this.onClick
      })
    );
  }
});

module.exports = SidebarToggle;
