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
  const { input, span, td } = React.DOM;
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
      renderValue: PropTypes.func.isRequired,
      enableInput: PropTypes.bool,
    },

    getInitialState: function () {
      return {
        inputEnabled: false,
      };
    },

    /**
     * Optimize cell rendering. Rerender cell content only if
     * the value or expanded state changes.
     */
    shouldComponentUpdate: function (nextProps, nextState) {
      return (this.props.value != nextProps.value) ||
        (this.state !== nextState) ||
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

    updateInputEnabled: function (evt) {
      this.setState(Object.assign({}, this.state, {
        inputEnabled: evt.target.nodeName !== "input",
      }));
    },

    render: function () {
      let {
        member,
        id,
        value,
        decorator,
        renderValue,
        enableInput,
      } = this.props;
      let type = member.type || "";

      // Compute class name list for the <td> element.
      let classNames = this.getCellClass(member.object, id) || [];
      classNames.push("treeValueCell");
      classNames.push(type + "Cell");

      // Render value using a default render function or custom
      // provided function from props or a decorator.
      renderValue = renderValue || defaultRenderValue;
      if (decorator && decorator.renderValue) {
        renderValue = decorator.renderValue(member.object, id) || renderValue;
      }

      let props = Object.assign({}, this.props, {
        object: value,
      });

      let cellElement;
      if (enableInput && this.state.inputEnabled && type !== "object") {
        classNames.push("inputEnabled");
        cellElement = input({
          autoFocus: true,
          onBlur: this.updateInputEnabled,
          readOnly: true,
          value,
          "aria-labelledby": id
        });
      } else {
        cellElement = span({
          onClick: (type !== "object") ? this.updateInputEnabled : null,
          "aria-labelledby": id
        },
          renderValue(props)
        );
      }

      // Render me!
      return (
        td({
          className: classNames.join(" "),
          role: "presentation"
        },
          cellElement
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
