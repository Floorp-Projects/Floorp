/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  const React = require("devtools/client/shared/vendor/react");

  // Shortcuts
  const { td, span } = React.DOM;
  const PropTypes = React.PropTypes;

  /**
   * This template represents a cell in TreeView row. It's rendered
   * using <td> element (the row is <tr> and the entire tree is <table>).
   */
  let TreeCell = React.createClass({
    displayName: "TreeCell",

    // See TreeView component for detailed property explanation.
    propTypes: {
      value: PropTypes.any,
      decorator: PropTypes.object,
      id: PropTypes.string.isRequired,
      member: PropTypes.object.isRequired,
      renderValue: PropTypes.func.isRequired
    },

    /**
     * Optimize cell rendering. Rerender cell content only if
     * the value or expanded state changes.
     */
    shouldComponentUpdate: function (nextProps) {
      return (this.props.value != nextProps.value) ||
        (this.props.member.open != nextProps.member.open);
    },

    getCellClass: function (object, id) {
      let decorator = this.props.decorator;
      if (!decorator || !decorator.getCellClass) {
        return [];
      }

      // Decorator can return a simple string or array of strings.
      let classNames = decorator.getCellClass(object, id);
      if (!classNames) {
        return [];
      }

      if (typeof classNames == "string") {
        classNames = [classNames];
      }

      return classNames;
    },

    render: function () {
      let member = this.props.member;
      let type = member.type || "";
      let id = this.props.id;
      let value = this.props.value;
      let decorator = this.props.decorator;

      // Compute class name list for the <td> element.
      let classNames = this.getCellClass(member.object, id) || [];
      classNames.push("treeValueCell");
      classNames.push(type + "Cell");

      // Render value using a default render function or custom
      // provided function from props or a decorator.
      let renderValue = this.props.renderValue || defaultRenderValue;
      if (decorator && decorator.renderValue) {
        renderValue = decorator.renderValue(member.object, id) || renderValue;
      }

      let props = Object.assign({}, this.props, {
        object: value,
      });

      // Render me!
      return (
        td({ className: classNames.join(" ") },
          span({}, renderValue(props))
        )
      );
    }
  });

  // Default value rendering.
  let defaultRenderValue = props => {
    return (
      props.object + ""
    );
  };

  // Exports from this module
  module.exports = TreeCell;
});
