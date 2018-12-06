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

const ConnectionPromptSetting = createFactory(require("./ConnectionPromptSetting"));
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

const Actions = require("../actions/index");
const { DEBUG_TARGET_PANE, PAGE_TYPES, RUNTIMES } = require("../constants");

const {
  getCurrentConnectionPromptSetting,
  getCurrentRuntimeInfo,
} = require("../modules/runtimes-state-helper");
const { isSupportedDebugTargetPane } = require("../modules/debug-target-support");

class RuntimePage extends PureComponent {
  static get propTypes() {
    return {
      collapsibilities: PropTypes.object.isRequired,
      connectionPromptEnabled: PropTypes.bool.isRequired,
      dispatch: PropTypes.func.isRequired,
      installedExtensions: PropTypes.arrayOf(PropTypes.object).isRequired,
      otherWorkers: PropTypes.arrayOf(PropTypes.object).isRequired,
      runtimeId: PropTypes.string.isRequired,
      runtimeInfo: PropTypes.object,
      serviceWorkers: PropTypes.arrayOf(PropTypes.object).isRequired,
      sharedWorkers: PropTypes.arrayOf(PropTypes.object).isRequired,
      tabs: PropTypes.arrayOf(PropTypes.object).isRequired,
      temporaryExtensions: PropTypes.arrayOf(PropTypes.object).isRequired,
    };
  }

  // TODO: avoid the use of this method
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1508688
  componentWillMount() {
    const { dispatch, runtimeId } = this.props;
    dispatch(Actions.selectPage(PAGE_TYPES.RUNTIME, runtimeId));
  }

  renderConnectionPromptSetting() {
    const { connectionPromptEnabled, dispatch } = this.props;

    return dom.div(
      {
        className: "connection-prompt-setting",
      },
      ConnectionPromptSetting({ connectionPromptEnabled, dispatch }),
    );
  }

  renderDebugTargetPane(name, targets, actionComponent,
                        detailComponent, paneKey, localizationId) {
    const { collapsibilities, dispatch, runtimeInfo } = this.props;

    if (!isSupportedDebugTargetPane(runtimeInfo.type, paneKey)) {
      return null;
    }

    return Localized(
      {
        id: localizationId,
        attrs: { name: true },
      },
      DebugTargetPane({
        actionComponent,
        collapsibilityKey: paneKey,
        detailComponent,
        dispatch,
        isCollapsed: collapsibilities.get(paneKey),
        name,
        targets,
      })
    );
  }

  render() {
    const {
      dispatch,
      installedExtensions,
      otherWorkers,
      runtimeId,
      runtimeInfo,
      serviceWorkers,
      sharedWorkers,
      tabs,
      temporaryExtensions,
    } = this.props;

    if (!runtimeInfo) {
      // runtimeInfo can be null when the selectPage action navigates from a runtime A
      // to a runtime B (between unwatchRuntime and watchRuntime).
      return null;
    }

    // do not show the connection prompt setting in 'This Firefox'
    const shallShowPromptSetting = runtimeId !== RUNTIMES.THIS_FIREFOX;

    return dom.article(
      {
        className: "page js-runtime-page",
      },
      RuntimeInfo(runtimeInfo),
      shallShowPromptSetting
        ? this.renderConnectionPromptSetting()
        : null,
      isSupportedDebugTargetPane(runtimeInfo.type, DEBUG_TARGET_PANE.TEMPORARY_EXTENSION)
        ? TemporaryExtensionInstaller({ dispatch })
        : null,
      this.renderDebugTargetPane("Temporary Extensions",
                                 temporaryExtensions,
                                 TemporaryExtensionAction,
                                 ExtensionDetail,
                                 DEBUG_TARGET_PANE.TEMPORARY_EXTENSION,
                                 "about-debugging-runtime-temporary-extensions"),
      this.renderDebugTargetPane("Extensions",
                                 installedExtensions,
                                 InspectAction,
                                 ExtensionDetail,
                                 DEBUG_TARGET_PANE.INSTALLED_EXTENSION,
                                 "about-debugging-runtime-extensions"),
      this.renderDebugTargetPane("Tabs",
                                 tabs,
                                 InspectAction,
                                 TabDetail,
                                 DEBUG_TARGET_PANE.TAB,
                                 "about-debugging-runtime-tabs"),
      this.renderDebugTargetPane("Service Workers",
                                 serviceWorkers,
                                 ServiceWorkerAction,
                                 WorkerDetail,
                                 DEBUG_TARGET_PANE.SERVICE_WORKER,
                                 "about-debugging-runtime-service-workers"),
      this.renderDebugTargetPane("Shared Workers",
                                 sharedWorkers,
                                 InspectAction,
                                 WorkerDetail,
                                 DEBUG_TARGET_PANE.SHARED_WORKER,
                                 "about-debugging-runtime-shared-workers"),
      this.renderDebugTargetPane("Other Workers",
                                 otherWorkers,
                                 InspectAction,
                                 WorkerDetail,
                                 DEBUG_TARGET_PANE.OTHER_WORKER,
                                 "about-debugging-runtime-other-workers"),
    );
  }
}

const mapStateToProps = state => {
  return {
    connectionPromptEnabled: getCurrentConnectionPromptSetting(state.runtimes),
    collapsibilities: state.ui.debugTargetCollapsibilities,
    installedExtensions: state.debugTargets.installedExtensions,
    otherWorkers: state.debugTargets.otherWorkers,
    runtimeInfo: getCurrentRuntimeInfo(state.runtimes),
    serviceWorkers: state.debugTargets.serviceWorkers,
    sharedWorkers: state.debugTargets.sharedWorkers,
    tabs: state.debugTargets.tabs,
    temporaryExtensions: state.debugTargets.temporaryExtensions,
  };
};

module.exports = connect(mapStateToProps)(RuntimePage);
