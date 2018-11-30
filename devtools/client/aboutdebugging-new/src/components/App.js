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

const Route = createFactory(require("devtools/client/shared/vendor/react-router-dom").Route);
const Switch = createFactory(require("devtools/client/shared/vendor/react-router-dom").Switch);
const Redirect = createFactory(require("devtools/client/shared/vendor/react-router-dom").Redirect);
const { withRouter } = require("devtools/client/shared/vendor/react-router-dom");

const Types = require("../types/index");
const { RUNTIMES } = require("../constants");

const ConnectPage = createFactory(require("./connect/ConnectPage"));
const RuntimePage = createFactory(require("./RuntimePage"));
const Sidebar = createFactory(require("./sidebar/Sidebar"));

class App extends PureComponent {
  static get propTypes() {
    return {
      adbAddonStatus: PropTypes.string,
      // The "dispatch" helper is forwarded to the App component via connect.
      // From that point, components are responsible for forwarding the dispatch
      // property to all components who need to dispatch actions.
      dispatch: PropTypes.func.isRequired,
      fluentBundles: PropTypes.arrayOf(PropTypes.object).isRequired,
      isScanningUsb: PropTypes.bool.isRequired,
      networkEnabled: PropTypes.bool.isRequired,
      networkLocations: PropTypes.arrayOf(PropTypes.string).isRequired,
      networkRuntimes: PropTypes.arrayOf(Types.runtime).isRequired,
      selectedPage: PropTypes.string,
      selectedRuntime: PropTypes.string,
      usbRuntimes: PropTypes.arrayOf(Types.runtime).isRequired,
      wifiEnabled: PropTypes.bool.isRequired,
    };
  }

  renderConnect() {
    const {
      adbAddonStatus,
      dispatch,
      networkEnabled,
      networkLocations,
      wifiEnabled,
    } = this.props;

    return ConnectPage({
      adbAddonStatus,
      dispatch,
      networkEnabled,
      networkLocations,
      wifiEnabled,
    });
  }

  // The `match` object here is passed automatically by the Route object.
  // We are using it to read the route path.
  // See react-router docs:
  // https://github.com/ReactTraining/react-router/blob/master/packages/react-router/docs/api/match.md
  renderRuntime({ match }) {
    // Redirect to This Firefox in these cases:
    // - If the runtimepage for a device is the first page shown (since we can't
    //   keep connections open between page reloads).
    // - If no runtimeId is given.
    // - If runtime is not found in the runtimes list (this is handled later)
    const isDeviceFirstPage =
      !this.props.selectedPage &&
      match.params.runtimeId !== RUNTIMES.THIS_FIREFOX;
    if (!match.params.runtimeId || isDeviceFirstPage) {
      return Redirect({ to: `/runtime/${RUNTIMES.THIS_FIREFOX}` });
    }

    const isRuntimeAvailable = id => {
      const runtimes = [
        ...this.props.networkRuntimes,
        ...this.props.usbRuntimes,
      ];
      return !!runtimes.find(x => x.id === id);
    };

    const { dispatch } = this.props;

    let runtimeId = match.params.runtimeId || RUNTIMES.THIS_FIREFOX;
    if (match.params.runtimeId !== RUNTIMES.THIS_FIREFOX) {
      const rawId = decodeURIComponent(match.params.runtimeId);
      if (isRuntimeAvailable(rawId)) {
        runtimeId = rawId;
      } else {
        // Also redirect to "This Firefox" if runtime is not found
        return Redirect({ to: `/runtime/${RUNTIMES.THIS_FIREFOX}` });
      }
    }

    // we need to pass a key so the component updates when we want to showcase
    // a different runtime
    return RuntimePage({ dispatch, key: runtimeId, runtimeId });
  }

  renderRoutes() {
    return Switch(
      {},
      Route({
        path: "/connect",
        render: () => this.renderConnect(),
      }),
      Route({
        path: "/runtime/:runtimeId",
        render: routeProps => this.renderRuntime(routeProps),
      }),
      Route({
        path: "/",
        exact: true,
        // will redirect to This Firefox
        render: routeProps => this.renderRuntime(routeProps),
      }),
      // default route when there's no match
      // TODO: maybe we'd like to do something else than a redirect. See:
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1509897
      Route({
        render: () => Redirect({ to: "/"}),
      })
    );
  }

  render() {
    const {
      adbAddonStatus,
      dispatch,
      fluentBundles,
      isScanningUsb,
      networkRuntimes,
      selectedPage,
      selectedRuntime,
      usbRuntimes,
    } = this.props;

    return LocalizationProvider(
      { messages: fluentBundles },
      dom.div(
        { className: "app" },
        Sidebar({
          adbAddonStatus,
          className: "app__sidebar",
          dispatch,
          isScanningUsb,
          networkRuntimes,
          selectedPage,
          selectedRuntime,
          usbRuntimes,
        }),
        dom.main({ className: "app__content" }, this.renderRoutes())
      )
    );
  }
}

const mapStateToProps = state => {
  return {
    adbAddonStatus: state.ui.adbAddonStatus,
    isScanningUsb: state.ui.isScanningUsb,
    networkEnabled: state.ui.networkEnabled,
    networkLocations: state.ui.networkLocations,
    networkRuntimes: state.runtimes.networkRuntimes,
    selectedPage: state.ui.selectedPage,
    selectedRuntime: state.ui.selectedRuntime,
    usbRuntimes: state.runtimes.usbRuntimes,
    wifiEnabled: state.ui.wifiEnabled,
  };
};

const mapDispatchToProps = dispatch => ({
  dispatch,
});

module.exports = withRouter(connect(mapStateToProps, mapDispatchToProps)(App));
