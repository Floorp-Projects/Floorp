/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { connect } = require("devtools/client/shared/vendor/react-redux");
const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { PAGES } = require("../constants");

const ConnectPage = createFactory(require("./ConnectPage"));
const RuntimePage = createFactory(require("./RuntimePage"));
const Sidebar = createFactory(require("./Sidebar"));

class App extends PureComponent {
  static get propTypes() {
    return {
      // The "dispatch" helper is forwarded to the App component via connect.
      // From that point, components are responsible for forwarding the dispatch
      // property to all components who need to dispatch actions.
      dispatch: PropTypes.func.isRequired,
      selectedPage: PropTypes.string.isRequired,
    };
  }

  getSelectedPageComponent() {
    const { dispatch, selectedPage } = this.props;

    switch (selectedPage) {
      case PAGES.THIS_FIREFOX:
        return RuntimePage({ dispatch });
      case PAGES.CONNECT:
        return ConnectPage();
      default:
        // Invalid page, blank.
        return null;
    }
  }

  render() {
    const { dispatch, selectedPage } = this.props;

    return dom.div(
      {
        className: "app",
      },
      Sidebar({ dispatch, selectedPage }),
      this.getSelectedPageComponent(),
    );
  }
}

const mapStateToProps = state => {
  return {
    selectedPage: state.ui.selectedPage,
  };
};

const mapDispatchToProps = dispatch => ({
  dispatch,
});

module.exports = connect(mapStateToProps, mapDispatchToProps)(App);
