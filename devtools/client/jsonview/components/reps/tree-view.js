/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

define(function(require, exports, module) {

// Dependencies
const React = require("react");
const { createFactories } = require("./rep-utils");
const { Rep } = createFactories(require("./rep"));
const { StringRep } = require("./string");
const DOM = React.DOM;

var uid = 0;

/**
 * Renders a tree view with expandable/collapsible items.
 */
var TreeView = React.createClass({
  displayName: "TreeView",

  getInitialState: function() {
    return {
      data: {},
      searchFilter: null
    };
  },

  // Rendering

  render: function() {
    var mode = this.props.mode;
    var root = this.state.data;

    var children = [];

    if (Array.isArray(root)) {
      for (var i=0; i<root.length; i++) {
        var child = root[i];
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
      DOM.div({className: "domTable", cellPadding: 0, cellSpacing: 0,
        onClick: this.onClick},
          children
      )
    );
  },

  // Data

  componentDidMount: function() {
    var members = initMembers(this.props.data, 0);
    this.setState({data: members, searchFilter: this.props.searchFilter});
  },

  componentWillReceiveProps: function(nextProps) {
    var updatedState = {
      searchFilter: nextProps.searchFilter
    };

    if (this.props.data !== nextProps.data) {
      updatedState.data = initMembers(nextProps.data, 0);
    }

    this.setState(updatedState);
  }
});

/**
 * Represents a node within the tree.
 */
var TreeNode = React.createFactory(React.createClass({
  displayName: "TreeNode",

  getInitialState: function() {
    return { data: {}, searchFilter: null };
  },

  componentDidMount: function() {
    this.setState({data: this.props.data});
  },

  render: function() {
    var member = this.state.data;
    var mode = this.props.mode;

    var classNames = ["memberRow"];
    classNames.push(member.type + "Row");

    if (member.hasChildren) {
      classNames.push("hasChildren");
    }

    if (member.open) {
      classNames.push("opened");
    }

    if (!member.children) {
      // Cropped strings are expandable, but they don't have children.
      var isString = typeof(member.value) == "string";
      if (member.hasChildren && !isString) {
        member.children = initMembers(member.value);
      } else {
        member.children = [];
      }
    }

    var children = [];
    if (member.open && member.children.length) {
      for (var i in member.children) {
        var child = member.children[i];
        children.push(TreeNode({
          key: child.key,
          data: child,
          mode: mode,
          searchFilter: this.state.searchFilter || this.props.searchFilter
        }));
      };
    }

    var filter = this.props.searchFilter || "";
    var name = member.name || "";
    var value = member.value || "";

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
      DOM.div({className: classNames.join(" "), onClick: this.onClick},
        DOM.span({className: "memberLabelCell"},
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
    )
  },

  onClick: function(e) {
    var member = this.state.data;
    member.open = !member.open;

    this.setState({data: member});

    e.stopPropagation();
  },
}));

// Helpers

function initMembers(parent) {
  var members = getMembers(parent);
  return members;
}

function getMembers(object) {
  var members = [];
  getObjectProperties(object, function(prop, value) {
    var valueType = typeof(value);
    var hasChildren = (valueType === "object" && hasProperties(value));

    // Cropped strings are expandable, so the user can see the
    // entire original value.
    if (StringRep.isCropped(value)) {
      hasChildren = true;
    }

    var type = getType(value);
    var member = createMember(type, prop, value, hasChildren);
    members.push(member);
  });

  return members;
}

function createMember(type, name, value, hasChildren) {
  var member = {
    name: name,
    type: type,
    rowClass: "memberRow-" + type,
    open: "",
    hasChildren: hasChildren,
    value: value,
    open: false,
    key: uid++
  };

  return member;
}

function getObjectProperties(obj, callback) {
  for (var p in obj) {
    try {
      callback.call(this, p, obj[p]);
    }
    catch (e) {
      console.error(e)
    }
  }
}

function hasProperties(obj) {
  if (typeof(obj) == "string") {
    return false;
  }

  try {
    for (var name in obj) {
      return true;
    }
  }
  catch (exc) {
  }

  return false;
}

function getType(object) {
  // A type provider (or a decorator) should be used here.
  return "dom";
}

// Exports from this module
exports.TreeView = TreeView;
});
