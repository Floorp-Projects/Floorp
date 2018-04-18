/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React & Redux
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { connect } = require("devtools/client/shared/vendor/react-redux");

const TreeView = createFactory(require("devtools/client/shared/components/tree/TreeView"));
// Reps
const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
const { Rep } = REPS;

const Grip = REPS.Grip;
// DOM Panel
const { GripProvider } = require("../grip-provider");

const { DomDecorator } = require("../dom-decorator");

/**
 * Renders DOM panel tree.
 */
class DomTree extends Component {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      filter: PropTypes.string,
      grips: PropTypes.object,
      object: PropTypes.any,
      openLink: PropTypes.func,
    };
  }

  constructor(props) {
    super(props);
    this.onFilter = this.onFilter.bind(this);
  }

  /**
   * Filter DOM properties. Return true if the object
   * should be visible in the tree.
   */
  onFilter(object) {
    if (!this.props.filter) {
      return true;
    }

    return (object.name && object.name.indexOf(this.props.filter) > -1);
  }

  /**
   * Render DOM panel content
   */
  render() {
    let {
      dispatch,
      grips,
      object,
      openLink,
    } = this.props;

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
        columns,
        decorator: new DomDecorator(),
        mode: MODE.SHORT,
        object,
        onFilter: this.onFilter,
        openLink,
        provider: new GripProvider(grips, dispatch),
        renderValue,
      })
    );
  }
}

const mapStateToProps = (state) => {
  return {
    grips: state.grips,
    filter: state.filter
  };
};

// Exports from this module
module.exports = connect(mapStateToProps)(DomTree);
