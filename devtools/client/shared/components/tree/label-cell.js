/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // ReactJS
  const React = require("devtools/client/shared/vendor/react");

  // Shortcuts
  const { td, span } = React.DOM;
  const PropTypes = React.PropTypes;

  /**
   * Render the default cell used for toggle buttons
   */
  let LabelCell = React.createClass({
    displayName: "LabelCell",

    // See the TreeView component for details related
    // to the 'member' object.
    propTypes: {
      member: PropTypes.object.isRequired
    },

    render: function () {
      let member = this.props.member;
      let level = member.level || 0;

      // Compute indentation dynamically. The deeper the item is
      // inside the hierarchy, the bigger is the left padding.
      let rowStyle = {
        "paddingLeft": (level * 16) + "px",
      };

      return (
        td({
          className: "treeLabelCell",
          key: "default",
          style: rowStyle},
          span({ className: "treeIcon" }),
          span({ className: "treeLabel " + member.type + "Label" },
            member.name
          )
        )
      );
    }
  });

  // Exports from this module
  module.exports = LabelCell;
});
