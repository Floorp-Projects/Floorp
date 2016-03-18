/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function(require, exports, module) {
  // Dependencies
  const React = require("devtools/client/shared/vendor/react");
  const { createFactories } = require("devtools/client/shared/components/reps/rep-utils");
  const { Rep } = createFactories(require("devtools/client/shared/components/reps/rep"));
  const { StringRep } = require("devtools/client/shared/components/reps/string");
  const DOM = React.DOM;

  let uid = 0;

  /**
   * Renders a tree view with expandable/collapsible items.
   */
  let TreeView = React.createClass({
    propTypes: {
      searchFilter: React.PropTypes.string,
      data: React.PropTypes.any,
      mode: React.PropTypes.string,
    },

    displayName: "TreeView",

    getInitialState: function() {
      return {
        data: {},
        searchFilter: null
      };
    },

    // Data

    componentDidMount: function() {
      let members = initMembers(this.props.data, 0);
      this.setState({ // eslint-disable-line
        data: members,
        searchFilter:
        this.props.searchFilter
      });
    },

    componentWillReceiveProps: function(nextProps) {
      let updatedState = {
        searchFilter: nextProps.searchFilter
      };

      if (this.props.data !== nextProps.data) {
        updatedState.data = initMembers(nextProps.data, 0);
      }

      this.setState(updatedState);
    },

    // Rendering

    render: function() {
      let mode = this.props.mode;
      let root = this.state.data;

      let children = [];

      if (Array.isArray(root)) {
        for (let i = 0; i < root.length; i++) {
          let child = root[i];
          children.push(TreeNode({
            key: child.key,
            data: child,
            mode: mode,
            searchFilter: this.state.searchFilter || this.props.searchFilter
          }));
        }
      } else {
        children.push(React.addons.createFragment(root));
      }

      return (
        DOM.div({className: "domTable", cellPadding: 0, cellSpacing: 0},
          children
        )
      );
    },
  });

  /**
   * Represents a node within the tree.
   */
  let TreeNode = React.createFactory(React.createClass({
    propTypes: {
      searchFilter: React.PropTypes.string,
      data: React.PropTypes.object,
      mode: React.PropTypes.string,
    },

    displayName: "TreeNode",

    getInitialState: function() {
      return {
        data: this.props.data,
        searchFilter: null
      };
    },

    onClick: function(e) {
      let member = this.state.data;
      member.open = !member.open;

      this.setState({data: member});

      e.stopPropagation();
    },

    render: function() {
      let member = this.state.data;
      let mode = this.props.mode;

      let classNames = ["memberRow"];
      classNames.push(member.type + "Row");

      if (member.hasChildren) {
        classNames.push("hasChildren");
      }

      if (member.open) {
        classNames.push("opened");
      }

      if (!member.children) {
        // Cropped strings are expandable, but they don't have children.
        let isString = typeof (member.value) == "string";
        if (member.hasChildren && !isString) {
          member.children = initMembers(member.value);
        } else {
          member.children = [];
        }
      }

      let children = [];
      if (member.open && member.children.length) {
        for (let i in member.children) {
          let child = member.children[i];
          children.push(TreeNode({
            key: child.key,
            data: child,
            mode: mode,
            searchFilter: this.state.searchFilter || this.props.searchFilter
          }));
        }
      }

      let filter = this.props.searchFilter || "";
      let name = member.name || "";
      let value = member.value || "";

      // Filtering is case-insensitive
      filter = filter.toLowerCase();
      name = name.toLowerCase();

      if (filter && (name.indexOf(filter) < 0)) {
        // Cache the stringify result, so the filtering is fast
        // the next time.
        if (!member.valueString) {
          member.valueString = JSON.stringify(value).toLowerCase();
        }

        if (member.valueString && member.valueString.indexOf(filter) < 0) {
          classNames.push("hidden");
        }
      }

      return (
        DOM.div({className: classNames.join(" ")},
          DOM.span({className: "memberLabelCell", onClick: this.onClick},
            DOM.span({className: "memberIcon"}),
            DOM.span({className: "memberLabel " + member.type + "Label"},
              member.name)
          ),
          DOM.span({className: "memberValueCell"},
            DOM.span({},
              Rep({
                object: member.value,
                mode: this.props.mode,
                member: member
              })
            )
          ),
          DOM.div({className: "memberChildren"},
            children
          )
        )
      );
    },
  }));

  // Helpers

  function initMembers(parent) {
    let members = getMembers(parent);
    return members;
  }

  function getMembers(object) {
    let members = [];
    getObjectProperties(object, function(prop, value) {
      let valueType = typeof (value);
      let hasChildren = (valueType === "object" && hasProperties(value));

      // Cropped strings are expandable, so the user can see the
      // entire original value.
      if (StringRep.isCropped(value)) {
        hasChildren = true;
      }

      let type = getType(value);
      let member = createMember(type, prop, value, hasChildren);
      members.push(member);
    });

    return members;
  }

  function createMember(type, name, value, hasChildren) {
    let member = {
      name: name,
      type: type,
      rowClass: "memberRow-" + type,
      hasChildren: hasChildren,
      value: value,
      open: false,
      key: uid++
    };

    return member;
  }

  function getObjectProperties(obj, callback) {
    for (let p in obj) {
      try {
        callback.call(this, p, obj[p]);
      } catch (e) {
        // Ignore
      }
    }
  }

  function hasProperties(obj) {
    if (typeof (obj) == "string") {
      return false;
    }

    return Object.keys(obj).length > 1;
  }

  function getType(object) {
    // A type provider (or a decorator) should be used here.
    return "dom";
  }

  // Exports from this module
  exports.TreeView = TreeView;
});
