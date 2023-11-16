/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  const { Component } = require("devtools/client/shared/vendor/react");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const dom = require("devtools/client/shared/vendor/react-dom-factories");
  const { input, span, td } = dom;

  /**
   * This template represents a cell in TreeView row. It's rendered
   * using <td> element (the row is <tr> and the entire tree is <table>).
   */
  class TreeCell extends Component {
    // See TreeView component for detailed property explanation.
    static get propTypes() {
      return {
        value: PropTypes.any,
        decorator: PropTypes.object,
        id: PropTypes.string.isRequired,
        member: PropTypes.object.isRequired,
        renderValue: PropTypes.func.isRequired,
        enableInput: PropTypes.bool,
      };
    }

    constructor(props) {
      super(props);

      this.state = {
        inputEnabled: false,
      };

      this.getCellClass = this.getCellClass.bind(this);
      this.updateInputEnabled = this.updateInputEnabled.bind(this);
    }

    /**
     * Optimize cell rendering. Rerender cell content only if
     * the value or expanded state changes.
     */
    shouldComponentUpdate(nextProps, nextState) {
      return (
        this.props.value !== nextProps.value ||
        this.state !== nextState ||
        this.props.member.open !== nextProps.member.open
      );
    }

    getCellClass(object, id) {
      const decorator = this.props.decorator;
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
    }

    updateInputEnabled(evt) {
      this.setState(
        Object.assign({}, this.state, {
          inputEnabled: evt.target.nodeName.toLowerCase() !== "input",
        })
      );
    }

    render() {
      let { member, id, value, decorator, renderValue, enableInput } =
        this.props;
      const type = member.type || "";

      // Compute class name list for the <td> element.
      const classNames = this.getCellClass(member.object, id) || [];
      classNames.push("treeValueCell");
      classNames.push(type + "Cell");

      // Render value using a default render function or custom
      // provided function from props or a decorator.
      renderValue = renderValue || defaultRenderValue;
      if (decorator?.renderValue) {
        renderValue = decorator.renderValue(member.object, id) || renderValue;
      }

      const props = Object.assign({}, this.props, {
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
          "aria-labelledby": id,
        });
      } else {
        cellElement = span(
          {
            onClick: type !== "object" ? this.updateInputEnabled : null,
            "aria-labelledby": id,
          },
          renderValue(props)
        );
      }

      // Render me!
      return td(
        {
          className: classNames.join(" "),
          role: "presentation",
        },
        cellElement
      );
    }
  }

  // Default value rendering.
  const defaultRenderValue = props => {
    return props.object + "";
  };

  // Exports from this module
  module.exports = TreeCell;
});
