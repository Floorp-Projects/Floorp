/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Make this available to both AMD and CJS environments
define(function(require, exports, module) {
  const { Component } = require("devtools/client/shared/vendor/react");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const dom = require("devtools/client/shared/vendor/react-dom-factories");
  const { thead, tr, td, div } = dom;

  /**
   * This component is responsible for rendering tree header.
   * It's based on <thead> element.
   */
  class TreeHeader extends Component {
    // See also TreeView component for detailed info about properties.
    static get propTypes() {
      return {
        // Custom tree decorator
        decorator: PropTypes.object,
        // True if the header should be visible
        header: PropTypes.bool,
        // Array with column definition
        columns: PropTypes.array
      };
    }

    static get defaultProps() {
      return {
        columns: [{
          id: "default"
        }]
      };
    }

    constructor(props) {
      super(props);
      this.getHeaderClass = this.getHeaderClass.bind(this);
    }

    getHeaderClass(colId) {
      const decorator = this.props.decorator;
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
    }

    render() {
      const cells = [];
      const visible = this.props.header;

      // Render the rest of the columns (if any)
      this.props.columns.forEach(col => {
        const cellStyle = {
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
            visible ? div({
              className: "treeHeaderCellBox",
              role: "presentation"
            }, col.title) : null
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
  }

  // Exports from this module
  module.exports = TreeHeader;
});
