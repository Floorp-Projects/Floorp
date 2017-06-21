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

  // Reps
  const { ObjectProvider } = require("./object-provider");
  const TreeRow = React.createFactory(require("./tree-row"));
  const TreeHeader = React.createFactory(require("./tree-header"));

  // Shortcuts
  const DOM = React.DOM;
  const PropTypes = React.PropTypes;

  /**
   * This component represents a tree view with expandable/collapsible nodes.
   * The tree is rendered using <table> element where every node is represented
   * by <tr> element. The tree is one big table where nodes (rows) are properly
   * indented from the left to mimic hierarchical structure of the data.
   *
   * The tree can have arbitrary number of columns and so, might be use
   * as an expandable tree-table UI widget as well. By default, there is
   * one column for node label and one for node value.
   *
   * The tree is maintaining its (presentation) state, which consists
   * from list of expanded nodes and list of columns.
   *
   * Complete data provider interface:
   * var TreeProvider = {
   *   getChildren: function(object);
   *   hasChildren: function(object);
   *   getLabel: function(object, colId);
   *   getValue: function(object, colId);
   *   getKey: function(object);
   *   getType: function(object);
   * }
   *
   * Complete tree decorator interface:
   * var TreeDecorator = {
   *   getRowClass: function(object);
   *   getCellClass: function(object, colId);
   *   getHeaderClass: function(colId);
   *   renderValue: function(object, colId);
   *   renderRow: function(object);
   *   renderCell: function(object, colId);
   *   renderLabelCell: function(object);
   * }
   */
  let TreeView = React.createClass({
    displayName: "TreeView",

    // The only required property (not set by default) is the input data
    // object that is used to puputate the tree.
    propTypes: {
      // The input data object.
      object: PropTypes.any,
      className: PropTypes.string,
      label: PropTypes.string,
      // Data provider (see also the interface above)
      provider: PropTypes.shape({
        getChildren: PropTypes.func,
        hasChildren: PropTypes.func,
        getLabel: PropTypes.func,
        getValue: PropTypes.func,
        getKey: PropTypes.func,
        getType: PropTypes.func,
      }).isRequired,
      // Tree decorator (see also the interface above)
      decorator: PropTypes.shape({
        getRowClass: PropTypes.func,
        getCellClass: PropTypes.func,
        getHeaderClass: PropTypes.func,
        renderValue: PropTypes.func,
        renderRow: PropTypes.func,
        renderCell: PropTypes.func,
        renderLabelCell: PropTypes.func,
      }),
      // Custom tree row (node) renderer
      renderRow: PropTypes.func,
      // Custom cell renderer
      renderCell: PropTypes.func,
      // Custom value renderef
      renderValue: PropTypes.func,
      // Custom tree label (including a toggle button) renderer
      renderLabelCell: PropTypes.func,
      // Set of expanded nodes
      expandedNodes: PropTypes.object,
      // Custom filtering callback
      onFilter: PropTypes.func,
      // Custom sorting callback
      onSort: PropTypes.func,
      // A header is displayed if set to true
      header: PropTypes.bool,
      // Long string is expandable by a toggle button
      expandableStrings: PropTypes.bool,
      // Array of columns
      columns: PropTypes.arrayOf(PropTypes.shape({
        id: PropTypes.string.isRequired,
        title: PropTypes.string,
        width: PropTypes.string
      }))
    },

    getDefaultProps: function () {
      return {
        object: null,
        renderRow: null,
        provider: ObjectProvider,
        expandedNodes: new Set(),
        expandableStrings: true,
        columns: []
      };
    },

    getInitialState: function () {
      return {
        expandedNodes: this.props.expandedNodes,
        columns: ensureDefaultColumn(this.props.columns),
        selected: null
      };
    },

    componentWillReceiveProps: function (nextProps) {
      let { expandedNodes } = nextProps;
      this.setState(Object.assign({}, this.state, {
        expandedNodes,
      }));
    },

    componentDidUpdate: function () {
      let selected = this.getSelectedRow(this.rows);
      if (!selected && this.rows.length > 0) {
        // TODO: Do better than just selecting the first row again. We want to
        // select (in order) previous, next or parent in case when selected
        // row is removed.
        this.selectRow(this.rows[0].props.member.path);
      }
    },

    // Node expand/collapse

    toggle: function (nodePath) {
      let nodes = this.state.expandedNodes;
      if (this.isExpanded(nodePath)) {
        nodes.delete(nodePath);
      } else {
        nodes.add(nodePath);
      }

      // Compute new state and update the tree.
      this.setState(Object.assign({}, this.state, {
        expandedNodes: nodes
      }));
    },

    isExpanded: function (nodePath) {
      return this.state.expandedNodes.has(nodePath);
    },

    // Event Handlers

    onKeyDown: function (event) {
      if (["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight"].includes(
        event.key)) {
        event.preventDefault();
      }
    },

    onKeyUp: function (event) {
      let row = this.getSelectedRow(this.rows);
      if (!row) {
        return;
      }

      let index = this.rows.indexOf(row);
      switch (event.key) {
        case "ArrowRight":
          let { hasChildren, open } = row.props.member;
          if (hasChildren && !open) {
            this.toggle(this.state.selected);
          }
          break;
        case "ArrowLeft":
          if (row && row.props.member.open) {
            this.toggle(this.state.selected);
          }
          break;
        case "ArrowDown":
          let nextRow = this.rows[index + 1];
          if (nextRow) {
            this.selectRow(nextRow.props.member.path);
          }
          break;
        case "ArrowUp":
          let previousRow = this.rows[index - 1];
          if (previousRow) {
            this.selectRow(previousRow.props.member.path);
          }
          break;
        default:
          return;
      }

      event.preventDefault();
    },

    onClickRow: function (nodePath, event) {
      event.stopPropagation();
      this.toggle(nodePath);
      this.selectRow(nodePath);
    },

    getSelectedRow: function (rows) {
      if (!this.state.selected || rows.length === 0) {
        return null;
      }
      return rows.find(row => this.isSelected(row.props.member.path));
    },

    selectRow: function (nodePath) {
      this.setState(Object.assign({}, this.state, {
        selected: nodePath
      }));
    },

    isSelected: function (nodePath) {
      return nodePath === this.state.selected;
    },

    // Filtering & Sorting

    /**
     * Filter out nodes that don't correspond to the current filter.
     * @return {Boolean} true if the node should be visible otherwise false.
     */
    onFilter: function (object) {
      let onFilter = this.props.onFilter;
      return onFilter ? onFilter(object) : true;
    },

    onSort: function (parent, children) {
      let onSort = this.props.onSort;
      return onSort ? onSort(parent, children) : children;
    },

    // Members

    /**
     * Return children node objects (so called 'members') for given
     * parent object.
     */
    getMembers: function (parent, level, path) {
      // Strings don't have children. Note that 'long' strings are using
      // the expander icon (+/-) to display the entire original value,
      // but there are no child items.
      if (typeof parent == "string") {
        return [];
      }

      let { expandableStrings, provider } = this.props;
      let children = provider.getChildren(parent) || [];

      // If the return value is non-array, the children
      // are being loaded asynchronously.
      if (!Array.isArray(children)) {
        return children;
      }

      children = this.onSort(parent, children) || children;

      return children.map(child => {
        let key = provider.getKey(child);
        let nodePath = TreeView.subPath(path, key);
        let type = provider.getType(child);
        let hasChildren = provider.hasChildren(child);

        // Value with no column specified is used for optimization.
        // The row is re-rendered only if this value changes.
        // Value for actual column is get when a cell is rendered.
        let value = provider.getValue(child);

        if (expandableStrings && isLongString(value)) {
          hasChildren = true;
        }

        // Return value is a 'member' object containing meta-data about
        // tree node. It describes node label, value, type, etc.
        return {
          // An object associated with this node.
          object: child,
          // A label for the child node
          name: provider.getLabel(child),
          // Data type of the child node (used for CSS customization)
          type: type,
          // Class attribute computed from the type.
          rowClass: "treeRow-" + type,
          // Level of the child within the hierarchy (top == 0)
          level: level,
          // True if this node has children.
          hasChildren: hasChildren,
          // Value associated with this node (as provided by the data provider)
          value: value,
          // True if the node is expanded.
          open: this.isExpanded(nodePath),
          // Node path
          path: nodePath,
          // True if the node is hidden (used for filtering)
          hidden: !this.onFilter(child),
          // True if the node is selected with keyboard
          selected: this.isSelected(nodePath)
        };
      });
    },

    /**
     * Render tree rows/nodes.
     */
    renderRows: function (parent, level = 0, path = "") {
      let rows = [];
      let decorator = this.props.decorator;
      let renderRow = this.props.renderRow || TreeRow;

      // Get children for given parent node, iterate over them and render
      // a row for every one. Use row template (a component) from properties.
      // If the return value is non-array, the children are being loaded
      // asynchronously.
      let members = this.getMembers(parent, level, path);
      if (!Array.isArray(members)) {
        return members;
      }

      members.forEach(member => {
        if (decorator && decorator.renderRow) {
          renderRow = decorator.renderRow(member.object) || renderRow;
        }

        let props = Object.assign({}, this.props, {
          key: member.path,
          member: member,
          columns: this.state.columns,
          id: member.path,
          ref: row => row && this.rows.push(row),
          onClick: this.onClickRow.bind(this, member.path)
        });

        // Render single row.
        rows.push(renderRow(props));

        // If a child node is expanded render its rows too.
        if (member.hasChildren && member.open) {
          let childRows = this.renderRows(member.object, level + 1,
            member.path);

          // If children needs to be asynchronously fetched first,
          // set 'loading' property to the parent row. Otherwise
          // just append children rows to the array of all rows.
          if (!Array.isArray(childRows)) {
            let lastIndex = rows.length - 1;
            props.member.loading = true;
            rows[lastIndex] = React.cloneElement(rows[lastIndex], props);
          } else {
            rows = rows.concat(childRows);
          }
        }
      });

      return rows;
    },

    render: function () {
      let root = this.props.object;
      let classNames = ["treeTable"];
      this.rows = [];

      // Use custom class name from props.
      let className = this.props.className;
      if (className) {
        classNames.push(...className.split(" "));
      }

      // Alright, let's render all tree rows. The tree is one big <table>.
      let rows = this.renderRows(root, 0, "");

      // This happens when the view needs to do initial asynchronous
      // fetch for the root object. The tree might provide a hook API
      // for rendering animated spinner (just like for tree nodes).
      if (!Array.isArray(rows)) {
        rows = [];
      }

      let props = Object.assign({}, this.props, {
        columns: this.state.columns
      });

      return (
        DOM.table({
          className: classNames.join(" "),
          role: "tree",
          tabIndex: 0,
          onKeyDown: this.onKeyDown,
          onKeyUp: this.onKeyUp,
          "aria-label": this.props.label || "",
          "aria-activedescendant": this.state.selected,
          cellPadding: 0,
          cellSpacing: 0},
          TreeHeader(props),
          DOM.tbody({
            role: "presentation"
          }, rows)
        )
      );
    }
  });

  TreeView.subPath = function (path, subKey) {
    return path + "/" + String(subKey).replace(/[\\/]/g, "\\$&");
  };

  /**
   * Creates a set with the paths of the nodes that should be expanded by default
   * according to the passed options.
   * @param {Object} The root node of the tree.
   * @param {Object} [optional] An object with the following optional parameters:
   *   - maxLevel: nodes nested deeper than this level won't be expanded.
   *   - maxNodes: maximum number of nodes that can be expanded. The traversal is
         breadth-first, so expanding nodes nearer to the root will be preferred.
         Sibling nodes will either be all expanded or none expanded.
   * }
   */
  TreeView.getExpandedNodes = function (rootObj,
    { maxLevel = Infinity, maxNodes = Infinity } = {}
  ) {
    let expandedNodes = new Set();
    let queue = [{
      object: rootObj,
      level: 1,
      path: ""
    }];
    while (queue.length) {
      let {object, level, path} = queue.shift();
      if (Object(object) !== object) {
        continue;
      }
      let keys = Object.keys(object);
      if (expandedNodes.size + keys.length > maxNodes) {
        // Avoid having children half expanded.
        break;
      }
      for (let key of keys) {
        let nodePath = TreeView.subPath(path, key);
        expandedNodes.add(nodePath);
        if (level < maxLevel) {
          queue.push({
            object: object[key],
            level: level + 1,
            path: nodePath
          });
        }
      }
    }
    return expandedNodes;
  };

  // Helpers

  /**
   * There should always be at least one column (the one with toggle buttons)
   * and this function ensures that it's true.
   */
  function ensureDefaultColumn(columns) {
    if (!columns) {
      columns = [];
    }

    let defaultColumn = columns.filter(col => col.id == "default");
    if (defaultColumn.length) {
      return columns;
    }

    // The default column is usually the first one.
    return [{id: "default"}, ...columns];
  }

  function isLongString(value) {
    return typeof value == "string" && value.length > 50;
  }

  // Exports from this module
  module.exports = TreeView;
});
