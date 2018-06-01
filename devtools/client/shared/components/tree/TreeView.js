/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Make this available to both AMD and CJS environments
define(function(require, exports, module) {
  const { cloneElement, Component, createFactory } =
    require("devtools/client/shared/vendor/react");
  const { findDOMNode } = require("devtools/client/shared/vendor/react-dom");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const dom = require("devtools/client/shared/vendor/react-dom-factories");

  // Reps
  const { ObjectProvider } = require("./ObjectProvider");
  const TreeRow = createFactory(require("./TreeRow"));
  const TreeHeader = createFactory(require("./TreeHeader"));

  const SUPPORTED_KEYS = [
    "ArrowUp",
    "ArrowDown",
    "ArrowLeft",
    "ArrowRight",
    "End",
    "Home"
  ];

  const defaultProps = {
    object: null,
    renderRow: null,
    provider: ObjectProvider,
    expandedNodes: new Set(),
    selected: null,
    expandableStrings: true,
    columns: []
  };

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
  class TreeView extends Component {
    // The only required property (not set by default) is the input data
    // object that is used to populate the tree.
    static get propTypes() {
      return {
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
        // Custom value renderer
        renderValue: PropTypes.func,
        // Custom tree label (including a toggle button) renderer
        renderLabelCell: PropTypes.func,
        // Set of expanded nodes
        expandedNodes: PropTypes.object,
        // Selected node
        selected: PropTypes.string,
        // Custom filtering callback
        onFilter: PropTypes.func,
        // Custom sorting callback
        onSort: PropTypes.func,
        // Custom row click callback
        onClickRow: PropTypes.func,
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
      };
    }

    static get defaultProps() {
      return defaultProps;
    }

    constructor(props) {
      super(props);

      this.state = {
        expandedNodes: props.expandedNodes,
        columns: ensureDefaultColumn(props.columns),
        selected: props.selected,
        lastSelectedIndex: 0
      };

      this.toggle = this.toggle.bind(this);
      this.isExpanded = this.isExpanded.bind(this);
      this.onKeyDown = this.onKeyDown.bind(this);
      this.onClickRow = this.onClickRow.bind(this);
      this.getSelectedRow = this.getSelectedRow.bind(this);
      this.selectRow = this.selectRow.bind(this);
      this.isSelected = this.isSelected.bind(this);
      this.onFilter = this.onFilter.bind(this);
      this.onSort = this.onSort.bind(this);
      this.getMembers = this.getMembers.bind(this);
      this.renderRows = this.renderRows.bind(this);
    }

    componentWillReceiveProps(nextProps) {
      const { expandedNodes, selected } = nextProps;
      const state = {
        expandedNodes,
        lastSelectedIndex: this.getSelectedRowIndex()
      };

      if (selected) {
        state.selected = selected;
      }

      this.setState(Object.assign({}, this.state, state));
    }

    componentDidUpdate() {
      const selected = this.getSelectedRow();
      if (!selected && this.rows.length > 0) {
        this.selectRow(this.rows[
          Math.min(this.state.lastSelectedIndex, this.rows.length - 1)]);
      }
    }

    static subPath(path, subKey) {
      return path + "/" + String(subKey).replace(/[\\/]/g, "\\$&");
    }

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
    static getExpandedNodes(rootObj, { maxLevel = Infinity, maxNodes = Infinity } = {}) {
      const expandedNodes = new Set();
      const queue = [{
        object: rootObj,
        level: 1,
        path: ""
      }];
      while (queue.length) {
        const {object, level, path} = queue.shift();
        if (Object(object) !== object) {
          continue;
        }
        const keys = Object.keys(object);
        if (expandedNodes.size + keys.length > maxNodes) {
          // Avoid having children half expanded.
          break;
        }
        for (const key of keys) {
          const nodePath = TreeView.subPath(path, key);
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
    }

    // Node expand/collapse

    toggle(nodePath) {
      const nodes = this.state.expandedNodes;
      if (this.isExpanded(nodePath)) {
        nodes.delete(nodePath);
      } else {
        nodes.add(nodePath);
      }

      // Compute new state and update the tree.
      this.setState(Object.assign({}, this.state, {
        expandedNodes: nodes
      }));
    }

    isExpanded(nodePath) {
      return this.state.expandedNodes.has(nodePath);
    }

    // Event Handlers

    onKeyDown(event) {
      if (!SUPPORTED_KEYS.includes(event.key)) {
        return;
      }

      const row = this.getSelectedRow();
      if (!row) {
        return;
      }

      const index = this.rows.indexOf(row);
      switch (event.key) {
        case "ArrowRight":
          const { hasChildren, open } = row.props.member;
          if (hasChildren && !open) {
            this.toggle(this.state.selected);
          }
          break;
        case "ArrowLeft":
          if (row && row.props.member.open) {
            this.toggle(this.state.selected);
          } else {
            const parentRow = this.rows.slice(0, index).reverse().find(
              r => r.props.member.level < row.props.member.level);
            if (parentRow) {
              this.selectRow(parentRow);
            }
          }
          break;
        case "ArrowDown":
          const nextRow = this.rows[index + 1];
          if (nextRow) {
            this.selectRow(nextRow);
          }
          break;
        case "ArrowUp":
          const previousRow = this.rows[index - 1];
          if (previousRow) {
            this.selectRow(previousRow);
          }
          break;
        case "Home":
          const firstRow = this.rows[0];
          if (firstRow) {
            this.selectRow(firstRow);
          }
          break;
        case "End":
          const lastRow = this.rows[this.rows.length - 1];
          if (lastRow) {
            this.selectRow(lastRow);
          }
          break;
      }

      // Focus should always remain on the tree container itself.
      this.tree.focus();
      event.preventDefault();
    }

    onClickRow(nodePath, event) {
      const onClickRow = this.props.onClickRow;

      if (onClickRow) {
        onClickRow.call(this, nodePath, event);
        return;
      }

      event.stopPropagation();
      const cell = event.target.closest("td");
      if (cell && cell.classList.contains("treeLabelCell")) {
        this.toggle(nodePath);
      }
      this.selectRow(event.currentTarget);
    }

    getSelectedRow() {
      if (!this.state.selected || this.rows.length === 0) {
        return null;
      }
      return this.rows.find(row => this.isSelected(row.props.member.path));
    }

    getSelectedRowIndex() {
      const row = this.getSelectedRow();
      if (!row) {
        // If selected row is not found, return index of the first row.
        return 0;
      }

      return this.rows.indexOf(row);
    }

    selectRow(row) {
      row = findDOMNode(row);
      if (this.state.selected === row.id) {
        return;
      }

      this.setState(Object.assign({}, this.state, {
        selected: row.id
      }));
      row.scrollIntoView({block: "nearest"});
    }

    isSelected(nodePath) {
      return nodePath === this.state.selected;
    }

    // Filtering & Sorting

    /**
     * Filter out nodes that don't correspond to the current filter.
     * @return {Boolean} true if the node should be visible otherwise false.
     */
    onFilter(object) {
      const onFilter = this.props.onFilter;
      return onFilter ? onFilter(object) : true;
    }

    onSort(parent, children) {
      const onSort = this.props.onSort;
      return onSort ? onSort(parent, children) : children;
    }

    // Members

    /**
     * Return children node objects (so called 'members') for given
     * parent object.
     */
    getMembers(parent, level, path) {
      // Strings don't have children. Note that 'long' strings are using
      // the expander icon (+/-) to display the entire original value,
      // but there are no child items.
      if (typeof parent == "string") {
        return [];
      }

      const { expandableStrings, provider } = this.props;
      let children = provider.getChildren(parent) || [];

      // If the return value is non-array, the children
      // are being loaded asynchronously.
      if (!Array.isArray(children)) {
        return children;
      }

      children = this.onSort(parent, children) || children;

      return children.map(child => {
        const key = provider.getKey(child);
        const nodePath = TreeView.subPath(path, key);
        const type = provider.getType(child);
        let hasChildren = provider.hasChildren(child);

        // Value with no column specified is used for optimization.
        // The row is re-rendered only if this value changes.
        // Value for actual column is get when a cell is rendered.
        const value = provider.getValue(child);

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
    }

    /**
     * Render tree rows/nodes.
     */
    renderRows(parent, level = 0, path = "") {
      let rows = [];
      const decorator = this.props.decorator;
      let renderRow = this.props.renderRow || TreeRow;

      // Get children for given parent node, iterate over them and render
      // a row for every one. Use row template (a component) from properties.
      // If the return value is non-array, the children are being loaded
      // asynchronously.
      const members = this.getMembers(parent, level, path);
      if (!Array.isArray(members)) {
        return members;
      }

      members.forEach(member => {
        if (decorator && decorator.renderRow) {
          renderRow = decorator.renderRow(member.object) || renderRow;
        }

        const props = Object.assign({}, this.props, {
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
          const childRows = this.renderRows(member.object, level + 1,
            member.path);

          // If children needs to be asynchronously fetched first,
          // set 'loading' property to the parent row. Otherwise
          // just append children rows to the array of all rows.
          if (!Array.isArray(childRows)) {
            const lastIndex = rows.length - 1;
            props.member.loading = true;
            rows[lastIndex] = cloneElement(rows[lastIndex], props);
          } else {
            rows = rows.concat(childRows);
          }
        }
      });

      return rows;
    }

    render() {
      const root = this.props.object;
      const classNames = ["treeTable"];
      this.rows = [];

      // Use custom class name from props.
      const className = this.props.className;
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

      const props = Object.assign({}, this.props, {
        columns: this.state.columns
      });

      return (
        dom.table({
          className: classNames.join(" "),
          role: "tree",
          ref: tree => {
            this.tree = tree;
          },
          tabIndex: 0,
          onKeyDown: this.onKeyDown,
          "aria-label": this.props.label || "",
          "aria-activedescendant": this.state.selected,
          cellPadding: 0,
          cellSpacing: 0},
          TreeHeader(props),
          dom.tbody({
            role: "presentation",
            tabIndex: -1
          }, rows)
        )
      );
    }
  }

  // Helpers

  /**
   * There should always be at least one column (the one with toggle buttons)
   * and this function ensures that it's true.
   */
  function ensureDefaultColumn(columns) {
    if (!columns) {
      columns = [];
    }

    const defaultColumn = columns.filter(col => col.id == "default");
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
