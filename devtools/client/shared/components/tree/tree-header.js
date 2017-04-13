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
  const { thead, tr, td, div } = React.DOM;
  const PropTypes = React.PropTypes;

  /**
   * This component is responsible for rendering tree header.
   * It's based on <thead> element.
   */
  let TreeHeader = React.createClass({
    displayName: "TreeHeader",

    // See also TreeView component for detailed info about properties.
    propTypes: {
      // Custom tree decorator
      decorator: PropTypes.object,
      // True if the header should be visible
      header: PropTypes.bool,
      // Array with column definition
      columns: PropTypes.array
    },

    getDefaultProps: function () {
      return {
        columns: [{
          id: "default"
        }]
      };
    },

    getHeaderClass: function (colId) {
      let decorator = this.props.decorator;
      if (!decorator || !decorator.getHeaderClass) {
        return [];
      }

      // Decorator can return a simple string or array of strings.
      let classNames = decorator.getHeaderClass(colId);
      if (!classNames) {
        return [];
      }

      if (typeof classNames == "string") {
        classNames = [classNames];
      }

      return classNames;
    },

    render: function () {
      let cells = [];
      let visible = this.props.header;

      // Render the rest of the columns (if any)
      this.props.columns.forEach(col => {
        let cellStyle = {
          "width": col.width ? col.width : "",
        };

        let classNames = [];

        if (visible) {
          classNames = this.getHeaderClass(col.id);
          classNames.push("treeHeaderCell");
        }

        cells.push(
          td({
            className: classNames.join(" "),
            style: cellStyle,
            role: "presentation",
            id: col.id,
            key: col.id,
          },
            visible ? div({ className: "treeHeaderCellBox" }, col.title) : null
          )
        );
      });

      return (
        thead({
          role: "presentation"
        }, tr({
          className: visible ? "treeHeaderRow" : "",
          role: "presentation"
        }, cells))
      );
    }
  });

  // Exports from this module
  module.exports = TreeHeader;
});
