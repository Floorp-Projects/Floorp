/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { connect } = require("devtools/client/shared/vendor/react-redux");
const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

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

class RuntimePage extends PureComponent {
  static get propTypes() {
    return {
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
      DebugTargetPane({
        actionComponent: TemporaryExtensionAction,
        detailComponent: ExtensionDetail,
        dispatch,
        name: "Temporary Extensions",
        targets: temporaryExtensions,
      }),
      DebugTargetPane({
        actionComponent: InspectAction,
        detailComponent: ExtensionDetail,
        dispatch,
        name: "Extensions",
        targets: installedExtensions,
      }),
      DebugTargetPane({
        actionComponent: InspectAction,
        detailComponent: TabDetail,
        dispatch,
        name: "Tabs",
        targets: tabs
      }),
      DebugTargetPane({
        actionComponent: ServiceWorkerAction,
        detailComponent: WorkerDetail,
        dispatch,
        name: "Service Workers",
        targets: serviceWorkers
      }),
      DebugTargetPane({
        actionComponent: InspectAction,
        detailComponent: WorkerDetail,
        dispatch,
        name: "Shared Workers",
        targets: sharedWorkers
      }),
      DebugTargetPane({
        actionComponent: InspectAction,
        detailComponent: WorkerDetail,
        dispatch,
        name: "Other Workers",
        targets: otherWorkers
      }),
    );
  }
}

const mapStateToProps = state => {
  return {
    installedExtensions: state.runtime.installedExtensions,
    otherWorkers: state.runtime.otherWorkers,
    serviceWorkers: state.runtime.serviceWorkers,
    sharedWorkers: state.runtime.sharedWorkers,
    tabs: state.runtime.tabs,
    temporaryExtensions: state.runtime.temporaryExtensions,
  };
};

module.exports = connect(mapStateToProps)(RuntimePage);
