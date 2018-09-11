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

const DebugTargetPane = createFactory(require("./debugtarget/DebugTargetPane"));
const ExtensionDetail = createFactory(require("./debugtarget/ExtensionDetail"));
const InspectAction = createFactory(require("./debugtarget/InspectAction"));
const RuntimeInfo = createFactory(require("./RuntimeInfo"));
const ServiceWorkerAction = createFactory(require("./debugtarget/ServiceWorkerAction"));
const TabDetail = createFactory(require("./debugtarget/TabDetail"));
const TemporaryExtensionAction = createFactory(require("./debugtarget/TemporaryExtensionAction"));
const TemporaryExtensionInstaller =
  createFactory(require("./debugtarget/TemporaryExtensionInstaller"));
const WorkerDetail = createFactory(require("./debugtarget/WorkerDetail"));

const Services = require("Services");
const { DEBUG_TARGET_PANE } = require("../constants");

class RuntimePage extends PureComponent {
  static get propTypes() {
    return {
      collapsibilities: PropTypes.object.isRequired,
      dispatch: PropTypes.func.isRequired,
      installedExtensions: PropTypes.arrayOf(PropTypes.object).isRequired,
      otherWorkers: PropTypes.arrayOf(PropTypes.object).isRequired,
      serviceWorkers: PropTypes.arrayOf(PropTypes.object).isRequired,
      sharedWorkers: PropTypes.arrayOf(PropTypes.object).isRequired,
      tabs: PropTypes.arrayOf(PropTypes.object).isRequired,
      temporaryExtensions: PropTypes.arrayOf(PropTypes.object).isRequired,
    };
  }

  render() {
    const {
      collapsibilities,
      dispatch,
      installedExtensions,
      otherWorkers,
      serviceWorkers,
      sharedWorkers,
      tabs,
      temporaryExtensions,
    } = this.props;

    return dom.article(
      {
        className: "page",
      },
      RuntimeInfo({
        icon: "chrome://branding/content/icon64.png",
        name: Services.appinfo.name,
        version: Services.appinfo.version,
      }),
      TemporaryExtensionInstaller({ dispatch }),
      Localized(
        {
          id: "about-debugging-runtime-temporary-extensions",
          attrs: { name: true }
        },
        DebugTargetPane({
          actionComponent: TemporaryExtensionAction,
          collapsibilityKey: DEBUG_TARGET_PANE.TEMPORARY_EXTENSION,
          detailComponent: ExtensionDetail,
          dispatch,
          isCollapsed: collapsibilities.get(DEBUG_TARGET_PANE.TEMPORARY_EXTENSION),
          name: "Temporary Extensions",
          targets: temporaryExtensions,
        })
      ),
      Localized(
        {
          id: "about-debugging-runtime-extensions",
          attrs: { name: true }
        },
        DebugTargetPane({
          actionComponent: InspectAction,
          collapsibilityKey: DEBUG_TARGET_PANE.INSTALLED_EXTENSION,
          detailComponent: ExtensionDetail,
          dispatch,
          isCollapsed: collapsibilities.get(DEBUG_TARGET_PANE.INSTALLED_EXTENSION),
          name: "Extensions",
          targets: installedExtensions,
        })
      ),
      Localized(
        {
          id: "about-debugging-runtime-tabs",
          attrs: { name: true }
        },
        DebugTargetPane({
          actionComponent: InspectAction,
          collapsibilityKey: DEBUG_TARGET_PANE.TAB,
          detailComponent: TabDetail,
          dispatch,
          isCollapsed: collapsibilities.get(DEBUG_TARGET_PANE.TAB),
          name: "Tabs",
          targets: tabs
        })
      ),
      Localized(
        {
          id: "about-debugging-runtime-service-workers",
          attrs: { name: true }
        },
        DebugTargetPane({
          actionComponent: ServiceWorkerAction,
          collapsibilityKey: DEBUG_TARGET_PANE.SERVICE_WORKER,
          detailComponent: WorkerDetail,
          dispatch,
          isCollapsed: collapsibilities.get(DEBUG_TARGET_PANE.SERVICE_WORKER),
          name: "Service Workers",
          targets: serviceWorkers
        })
      ),
      Localized(
        {
          id: "about-debugging-runtime-shared-workers",
          attrs: { name: true }
        },
        DebugTargetPane({
          actionComponent: InspectAction,
          collapsibilityKey: DEBUG_TARGET_PANE.SHARED_WORKER,
          detailComponent: WorkerDetail,
          dispatch,
          isCollapsed: collapsibilities.get(DEBUG_TARGET_PANE.SHARED_WORKER),
          name: "Shared Workers",
          targets: sharedWorkers
        })
      ),
      Localized(
        {
          id: "about-debugging-runtime-other-workers",
          attrs: { name: true }
        },
        DebugTargetPane({
          actionComponent: InspectAction,
          collapsibilityKey: DEBUG_TARGET_PANE.OTHER_WORKER,
          detailComponent: WorkerDetail,
          dispatch,
          isCollapsed: collapsibilities.get(DEBUG_TARGET_PANE.OTHER_WORKER),
          name: "Other Workers",
          targets: otherWorkers
        })
      ),
    );
  }
}

const mapStateToProps = state => {
  return {
    collapsibilities: state.ui.debugTargetCollapsibilities,
    installedExtensions: state.runtime.installedExtensions,
    otherWorkers: state.runtime.otherWorkers,
    serviceWorkers: state.runtime.serviceWorkers,
    sharedWorkers: state.runtime.sharedWorkers,
    tabs: state.runtime.tabs,
    temporaryExtensions: state.runtime.temporaryExtensions,
  };
};

module.exports = connect(mapStateToProps)(RuntimePage);
