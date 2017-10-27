/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // ReactJS
  const { Component, DOM: dom, PropTypes } =
    require("devtools/client/shared/vendor/react");

  /**
   * Render the default cell used for toggle buttons
   */
  class LabelCell extends Component {
    // See the TreeView component for details related
    // to the 'member' object.
    static get propTypes() {
      return {
        id: PropTypes.string.isRequired,
        member: PropTypes.object.isRequired
      };
    }

    render() {
      let id = this.props.id;
      let member = this.props.member;
      let level = member.level || 0;

      // Compute indentation dynamically. The deeper the item is
      // inside the hierarchy, the bigger is the left padding.
      let rowStyle = {
        "paddingInlineStart": (level * 16) + "px",
      };

      let iconClassList = ["treeIcon"];
      if (member.hasChildren && member.loading) {
        iconClassList.push("devtools-throbber");
      } else if (member.hasChildren) {
        iconClassList.push("theme-twisty");
      }
      if (member.open) {
        iconClassList.push("open");
      }

      return (
        dom.td({
          className: "treeLabelCell",
          key: "default",
          style: rowStyle,
          role: "presentation"},
          dom.span({
            className: iconClassList.join(" "),
            role: "presentation"
          }),
          dom.span({
            className: "treeLabel " + member.type + "Label",
            "aria-labelledby": id,
            "data-level": level
          }, member.name)
        )
      );
    }
  }

  // Exports from this module
  module.exports = LabelCell;
});
