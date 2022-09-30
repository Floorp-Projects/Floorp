/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals DomProvider */

"use strict";

// React & Redux
const {
  Component,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
// DOM Panel
const DomTree = createFactory(
  require("resource://devtools/client/dom/content/components/DomTree.js")
);

const MainToolbar = createFactory(
  require("resource://devtools/client/dom/content/components/MainToolbar.js")
);
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
    const { filter, object } = this.props;

    return div(
      { className: "mainFrame" },
      MainToolbar({
        dispatch: this.props.dispatch,
        object: this.props.object,
      }),
      div(
        { className: "treeTableBox devtools-monospace" },
        DomTree({
          filter,
          object,
          openLink: url => DomProvider.openLink(url),
        })
      )
    );
  }
}

// Transform state into props
// Note: use https://github.com/faassen/reselect for better performance.
const mapStateToProps = state => {
  return {
    filter: state.filter,
  };
};

// Exports from this module
module.exports = connect(mapStateToProps)(MainFrame);
