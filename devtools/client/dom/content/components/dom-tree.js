/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React & Redux
const React = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");

// Reps
const { createFactories } = require("devtools/client/shared/components/reps/rep-utils");
const TreeView = React.createFactory(require("devtools/client/shared/components/tree/tree-view"));
const { Rep } = createFactories(require("devtools/client/shared/components/reps/rep"));
const { Grip } = require("devtools/client/shared/components/reps/grip");
const { MODE } = require("devtools/client/shared/components/reps/constants");

// DOM Panel
const { GripProvider } = require("../grip-provider");
const { DomDecorator } = require("../dom-decorator");

// Shortcuts
const PropTypes = React.PropTypes;

/**
 * Renders DOM panel tree.
 */
var DomTree = React.createClass({
  displayName: "DomTree",

  propTypes: {
    object: PropTypes.any,
    filter: PropTypes.string,
    dispatch: PropTypes.func.isRequired,
    grips: PropTypes.object,
  },

  /**
   * Filter DOM properties. Return true if the object
   * should be visible in the tree.
   */
  onFilter: function (object) {
    if (!this.props.filter) {
      return true;
    }

    return (object.name && object.name.indexOf(this.props.filter) > -1);
  },

  /**
   * Render DOM panel content
   */
  render: function () {
    let columns = [{
      "id": "value"
    }];

    // This is the integration point with Reps. The DomTree is using
    // Reps to render all values. The code also specifies default rep
    // used for data types that don't have its own specific template.
    let renderValue = props => {
      return Rep(Object.assign({}, props, {
        defaultRep: Grip,
        cropLimit: 50,
      }));
    };

    return (
      TreeView({
        object: this.props.object,
        provider: new GripProvider(this.props.grips, this.props.dispatch),
        decorator: new DomDecorator(),
        mode: MODE.SHORT,
        columns: columns,
        renderValue: renderValue,
        onFilter: this.onFilter
      })
    );
  }
});

const mapStateToProps = (state) => {
  return {
    grips: state.grips,
    filter: state.filter
  };
};

// Exports from this module
module.exports = connect(mapStateToProps)(DomTree);

