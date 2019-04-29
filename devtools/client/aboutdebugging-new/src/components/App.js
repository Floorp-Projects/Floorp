/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { connect } = require("devtools/client/shared/vendor/react-redux");
const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const Route = createFactory(require("devtools/client/shared/vendor/react-router-dom").Route);
const Switch = createFactory(require("devtools/client/shared/vendor/react-router-dom").Switch);
const Redirect = createFactory(require("devtools/client/shared/vendor/react-router-dom").Redirect);

const Types = require("../types/index");
const { PAGE_TYPES, RUNTIMES } = require("../constants");

const ConnectPage = createFactory(require("./connect/ConnectPage"));
const RuntimePage = createFactory(require("./RuntimePage"));
const Sidebar = createFactory(require("./sidebar/Sidebar"));

class App extends PureComponent {
  static get propTypes() {
    return {
      adbAddonStatus: Types.adbAddonStatus,
      // The "dispatch" helper is forwarded to the App component via connect.
      // From that point, components are responsible for forwarding the dispatch
      // property to all components who need to dispatch actions.
      dispatch: PropTypes.func.isRequired,
      // getString prop is injected by the withLocalization wrapper
      getString: PropTypes.func.isRequired,
      isAdbReady: PropTypes.bool.isRequired,
      isScanningUsb: PropTypes.bool.isRequired,
      networkLocations: PropTypes.arrayOf(Types.location).isRequired,
      networkRuntimes: PropTypes.arrayOf(Types.runtime).isRequired,
      selectedPage: Types.page,
      selectedRuntimeId: PropTypes.string,
      usbRuntimes: PropTypes.arrayOf(Types.runtime).isRequired,
    };
  }

  componentDidUpdate() {
    this.updateTitle();
  }

  updateTitle() {
    const { getString, selectedPage, selectedRuntimeId } = this.props;

    const pageTitle = selectedPage === PAGE_TYPES.RUNTIME ?
      getString("about-debugging-page-title-runtime-page", { selectedRuntimeId }) :
      getString("about-debugging-page-title-setup-page");

    document.title = pageTitle;
  }

  renderConnect() {
    const {
      adbAddonStatus,
      dispatch,
      networkLocations,
    } = this.props;

    return ConnectPage({
      adbAddonStatus,
      dispatch,
      networkLocations,
    });
  }

  // The `match` object here is passed automatically by the Route object.
  // We are using it to read the route path.
  // See react-router docs:
  // https://github.com/ReactTraining/react-router/blob/master/packages/react-router/docs/api/match.md
  renderRuntime({ match }) {
    const isRuntimeAvailable = id => {
      const runtimes = [
        ...this.props.networkRuntimes,
        ...this.props.usbRuntimes,
      ];
      const runtime = runtimes.find(x => x.id === id);
      return runtime && runtime.runtimeDetails;
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
        path: "/setup",
        render: () => this.renderConnect(),
      }),
      Route({
        path: "/runtime/:runtimeId",
        render: routeProps => this.renderRuntime(routeProps),
      }),
      // default route when there's no match which includes "/"
      // TODO: the url does not match "/" means invalid URL,
      // in this case maybe we'd like to do something else than a redirect.
      // See: https://bugzilla.mozilla.org/show_bug.cgi?id=1509897
      Route({
        render: routeProps => {
          const { pathname } = routeProps.location;
          // The old about:debugging supported the following routes:
          // about:debugging#workers, about:debugging#addons and about:debugging#tabs.
          // Such links can still be found in external documentation pages.
          // We redirect to This Firefox rather than the Setup Page here.
          if (pathname === "/workers" || pathname === "/addons" || pathname === "/tabs") {
            return Redirect({ to: `/runtime/${RUNTIMES.THIS_FIREFOX}`});
          }
          return Redirect({ to: "/setup"});
        },
      })
    );
  }

  render() {
    const {
      adbAddonStatus,
      dispatch,
      isAdbReady,
      isScanningUsb,
      networkRuntimes,
      selectedPage,
      selectedRuntimeId,
      usbRuntimes,
    } = this.props;

    return Localized(
      { },
      dom.div(
        { className: "app" },
        Sidebar({
          adbAddonStatus,
          className: "app__sidebar",
          dispatch,
          isAdbReady,
          isScanningUsb,
          networkRuntimes,
          selectedPage,
          selectedRuntimeId,
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
    isAdbReady: state.ui.isAdbReady,
    isScanningUsb: state.ui.isScanningUsb,
    networkLocations: state.ui.networkLocations,
    networkRuntimes: state.runtimes.networkRuntimes,
    selectedPage: state.ui.selectedPage,
    selectedRuntimeId: state.runtimes.selectedRuntimeId,
    usbRuntimes: state.runtimes.usbRuntimes,
  };
};

const mapDispatchToProps = dispatch => ({
  dispatch,
});

module.exports = FluentReact
  .withLocalization(
      connect(mapStateToProps, mapDispatchToProps)(App));
