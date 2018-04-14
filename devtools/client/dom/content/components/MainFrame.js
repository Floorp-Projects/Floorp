/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals DomProvider */

"use strict";

// React & Redux
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { connect } = require("devtools/client/shared/vendor/react-redux");
// DOM Panel
const DomTree = createFactory(require("./DomTree"));

const MainToolbar = createFactory(require("./MainToolbar"));
// Shortcuts
const { div } = dom;

/**
 * Renders basic layout of the DOM panel. The DOM panel content consists
 * from two main parts: toolbar and tree.
 */
class MainFrame extends Component {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      filter: PropTypes.string,
      object: PropTypes.any,
    };
  }

  /**
   * Render DOM panel content
   */
  render() {
    let {
      filter,
      object,
    } = this.props;

    return (
      div({className: "mainFrame"},
        MainToolbar({
          dispatch: this.props.dispatch,
          object: this.props.object
        }),
        div({className: "treeTableBox"},
          DomTree({
            filter,
            object,
            openLink: url => DomProvider.openLink(url),
          })
        )
      )
    );
  }
}

// Transform state into props
// Note: use https://github.com/faassen/reselect for better performance.
const mapStateToProps = (state) => {
  return {
    filter: state.filter
  };
};

// Exports from this module
module.exports = connect(mapStateToProps)(MainFrame);
