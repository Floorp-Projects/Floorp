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
  const ReactDOM = require("devtools/client/shared/vendor/react-dom");

  // Tree
  const TreeCell = React.createFactory(require("./TreeCell"));
  const LabelCell = React.createFactory(require("./LabelCell"));

  // Scroll
  const { scrollIntoViewIfNeeded } = require("devtools/client/shared/scroll");

  // Shortcuts
  const { tr } = React.DOM;
  const PropTypes = React.PropTypes;

  /**
   * This template represents a node in TreeView component. It's rendered
   * using <tr> element (the entire tree is one big <table>).
   */
  let TreeRow = React.createClass({
    displayName: "TreeRow",

    // See TreeView component for more details about the props and
    // the 'member' object.
    propTypes: {
      member: PropTypes.shape({
        object: PropTypes.obSject,
        name: PropTypes.sring,
        type: PropTypes.string.isRequired,
        rowClass: PropTypes.string.isRequired,
        level: PropTypes.number.isRequired,
        hasChildren: PropTypes.bool,
        value: PropTypes.any,
        open: PropTypes.bool.isRequired,
        path: PropTypes.string.isRequired,
        hidden: PropTypes.bool,
        selected: PropTypes.bool,
      }),
      decorator: PropTypes.object,
      renderCell: PropTypes.object,
      renderLabelCell: PropTypes.object,
      columns: PropTypes.array.isRequired,
      id: PropTypes.string.isRequired,
      provider: PropTypes.object.isRequired,
      onClick: PropTypes.func.isRequired,
      onMouseOver: PropTypes.func,
      onMouseOut: PropTypes.func
    },

    componentWillReceiveProps(nextProps) {
      // I don't like accessing the underlying DOM elements directly,
      // but this optimization makes the filtering so damn fast!
      // The row doesn't have to be re-rendered, all we really need
      // to do is toggling a class name.
      // The important part is that DOM elements don't need to be
      // re-created when they should appear again.
      if (nextProps.member.hidden != this.props.member.hidden) {
        let row = ReactDOM.findDOMNode(this);
        row.classList.toggle("hidden");
      }
    },

    /**
     * Optimize row rendering. If props are the same do not render.
     * This makes the rendering a lot faster!
     */
    shouldComponentUpdate: function (nextProps) {
      let props = ["name", "open", "value", "loading", "selected", "hasChildren"];
      for (let p in props) {
        if (nextProps.member[props[p]] != this.props.member[props[p]]) {
          return true;
        }
      }

      return false;
    },

    componentDidUpdate: function () {
      if (this.props.member.selected) {
        let row = ReactDOM.findDOMNode(this);
        // Because this is called asynchronously, context window might be
        // already gone.
        if (row.ownerDocument.defaultView) {
          scrollIntoViewIfNeeded(row);
        }
      }
    },

    getRowClass: function (object) {
      let decorator = this.props.decorator;
      if (!decorator || !decorator.getRowClass) {
        return [];
      }

      // Decorator can return a simple string or array of strings.
      let classNames = decorator.getRowClass(object);
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
      let decorator = this.props.decorator;
      let props = {
        id: this.props.id,
        role: "treeitem",
        "aria-level": member.level,
        "aria-selected": !!member.selected,
        onClick: this.props.onClick,
        onMouseOver: this.props.onMouseOver,
        onMouseOut: this.props.onMouseOut
      };

      // Compute class name list for the <tr> element.
      let classNames = this.getRowClass(member.object) || [];
      classNames.push("treeRow");
      classNames.push(member.type + "Row");

      if (member.hasChildren) {
        classNames.push("hasChildren");
        props["aria-expanded"] = false;
      }

      if (member.open) {
        classNames.push("opened");
        props["aria-expanded"] = true;
      }

      if (member.loading) {
        classNames.push("loading");
      }

      if (member.selected) {
        classNames.push("selected");
      }

      if (member.hidden) {
        classNames.push("hidden");
      }

      props.className = classNames.join(" ");

      // The label column (with toggle buttons) is usually
      // the first one, but there might be cases (like in
      // the Memory panel) where the toggling is done
      // in the last column.
      let cells = [];

      // Get components for rendering cells.
      let renderCell = this.props.renderCell || RenderCell;
      let renderLabelCell = this.props.renderLabelCell || RenderLabelCell;
      if (decorator && decorator.renderLabelCell) {
        renderLabelCell = decorator.renderLabelCell(member.object) ||
          renderLabelCell;
      }

      // Render a cell for every column.
      this.props.columns.forEach(col => {
        let cellProps = Object.assign({}, this.props, {
          key: col.id,
          id: col.id,
          value: this.props.provider.getValue(member.object, col.id)
        });

        if (decorator && decorator.renderCell) {
          renderCell = decorator.renderCell(member.object, col.id);
        }

        let render = (col.id == "default") ? renderLabelCell : renderCell;

        // Some cells don't have to be rendered. This happens when some
        // other cells span more columns. Note that the label cells contains
        // toggle buttons and should be usually there unless we are rendering
        // a simple non-expandable table.
        if (render) {
          cells.push(render(cellProps));
        }
      });

      // Render tree row
      return (
        tr(props, cells)
      );
    }
  });

  // Helpers

  let RenderCell = props => {
    return TreeCell(props);
  };

  let RenderLabelCell = props => {
    return LabelCell(props);
  };

  // Exports from this module
  module.exports = TreeRow;
});
