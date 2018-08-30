/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { connect } = require("devtools/client/shared/vendor/react-redux");
const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const LocalizationProvider = createFactory(FluentReact.LocalizationProvider);

const { PAGES } = require("../constants");

const ConnectPage = createFactory(require("./ConnectPage"));
const RuntimePage = createFactory(require("./RuntimePage"));
const Sidebar = createFactory(require("./sidebar/Sidebar"));

class App extends PureComponent {
  static get propTypes() {
    return {
      // The "dispatch" helper is forwarded to the App component via connect.
      // From that point, components are responsible for forwarding the dispatch
      // property to all components who need to dispatch actions.
      dispatch: PropTypes.func.isRequired,
      messageContexts: PropTypes.arrayOf(PropTypes.object).isRequired,
      networkLocations: PropTypes.arrayOf(PropTypes.string).isRequired,
      selectedPage: PropTypes.string.isRequired,
    };
  }

  getSelectedPageComponent() {
    const { dispatch, networkLocations, selectedPage } = this.props;

    switch (selectedPage) {
      case PAGES.THIS_FIREFOX:
        return RuntimePage({ dispatch });
      case PAGES.CONNECT:
        return ConnectPage({ networkLocations });
      default:
        // Invalid page, blank.
        return null;
    }
  }

  render() {
    const {
      dispatch,
      messageContexts,
      networkLocations,
      selectedPage,
    } = this.props;

    return LocalizationProvider(
      { messages: messageContexts },
      dom.div(
        { className: "app" },
        Sidebar({ dispatch, networkLocations, selectedPage }),
        this.getSelectedPageComponent()
      )
    );
  }
}

const mapStateToProps = state => {
  return {
    networkLocations: state.ui.networkLocations,
    selectedPage: state.ui.selectedPage,
  };
};

const mapDispatchToProps = dispatch => ({
  dispatch,
});

module.exports = connect(mapStateToProps, mapDispatchToProps)(App);
