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

const CompatibilityWarning = createFactory(require("./CompatibilityWarning"));
const ConnectionPromptSetting = createFactory(require("./ConnectionPromptSetting"));
const DebugTargetPane = createFactory(require("./debugtarget/DebugTargetPane"));
const ExtensionDebugSetting = createFactory(require("./ExtensionDebugSetting"));
const ExtensionDetail = createFactory(require("./debugtarget/ExtensionDetail"));
const InspectAction = createFactory(require("./debugtarget/InspectAction"));
const ProfilerDialog = createFactory(require("./ProfilerDialog"));
const RuntimeInfo = createFactory(require("./RuntimeInfo"));
const ServiceWorkerAction = createFactory(require("./debugtarget/ServiceWorkerAction"));
const ServiceWorkersWarning = createFactory(require("./ServiceWorkersWarning"));
const TabDetail = createFactory(require("./debugtarget/TabDetail"));
const TemporaryExtensionAction = createFactory(require("./debugtarget/TemporaryExtensionAction"));
const TemporaryExtensionDetail = createFactory(require("./debugtarget/TemporaryExtensionDetail"));
const TemporaryExtensionInstaller =
  createFactory(require("./debugtarget/TemporaryExtensionInstaller"));
const WorkerDetail = createFactory(require("./debugtarget/WorkerDetail"));

const Actions = require("../actions/index");
const { DEBUG_TARGET_PANE, PAGE_TYPES, RUNTIMES } = require("../constants");
const Types = require("../types/index");

const { getCurrentRuntimeDetails } = require("../modules/runtimes-state-helper");
const {
  isExtensionDebugSettingNeeded,
  isSupportedDebugTargetPane,
} = require("../modules/debug-target-support");

class RuntimePage extends PureComponent {
  static get propTypes() {
    return {
      collapsibilities: Types.collapsibilities.isRequired,
      dispatch: PropTypes.func.isRequired,
      installedExtensions: PropTypes.arrayOf(PropTypes.object).isRequired,
      otherWorkers: PropTypes.arrayOf(PropTypes.object).isRequired,
      runtimeDetails: Types.runtimeDetails,
      runtimeId: PropTypes.string.isRequired,
      serviceWorkers: PropTypes.arrayOf(PropTypes.object).isRequired,
      sharedWorkers: PropTypes.arrayOf(PropTypes.object).isRequired,
      showProfilerDialog: PropTypes.bool.isRequired,
      tabs: PropTypes.arrayOf(PropTypes.object).isRequired,
      temporaryExtensions: PropTypes.arrayOf(PropTypes.object).isRequired,
      temporaryInstallError: PropTypes.string,
    };
  }

  // TODO: avoid the use of this method
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1508688
  componentWillMount() {
    const { dispatch, runtimeId } = this.props;
    dispatch(Actions.selectPage(PAGE_TYPES.RUNTIME, runtimeId));
  }

  onProfilerButtonClick() {
    this.props.dispatch(Actions.showProfilerDialog());
  }

  renderRemoteRuntimeActions() {
    const { runtimeDetails, runtimeId, dispatch } = this.props;
    const { connectionPromptEnabled } = runtimeDetails;

    if (runtimeId === RUNTIMES.THIS_FIREFOX) {
      // Connection prompt and Profiling are only available on remote runtimes.
      return null;
    }

    return [
      dom.div(
        {
          className: "connection-prompt-setting",
          key: "connection-prompt-setting",
        },
        ConnectionPromptSetting({ connectionPromptEnabled, dispatch }),
      ),
      dom.p(
        {},
        Localized(
          {
            id: "about-debugging-runtime-profile-button",
            key: "profile-runtime-button",
          },
          dom.button(
            {
              className: "default-button js-profile-runtime-button",
              onClick: () => this.onProfilerButtonClick(),
            },
            "Profile Runtime"
          ),
        ),
      ),
    ];
  }

  renderDebugTargetPane(name, targets, actionComponent,
                        detailComponent, paneKey, localizationId) {
    const { collapsibilities, dispatch, runtimeDetails } = this.props;

    if (!isSupportedDebugTargetPane(runtimeDetails.info.type, paneKey)) {
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

  renderExtensionDebugSetting() {
    const { runtimeDetails, dispatch } = this.props;
    const { extensionDebugEnabled } = runtimeDetails;
    return ExtensionDebugSetting({ extensionDebugEnabled, dispatch });
  }

  render() {
    const {
      dispatch,
      installedExtensions,
      otherWorkers,
      runtimeDetails,
      serviceWorkers,
      sharedWorkers,
      showProfilerDialog,
      tabs,
      temporaryExtensions,
      temporaryInstallError,
    } = this.props;

    if (!runtimeDetails) {
      // runtimeInfo can be null when the selectPage action navigates from a runtime A
      // to a runtime B (between unwatchRuntime and watchRuntime).
      return null;
    }

    const { type } = runtimeDetails.info;
    const { compatibilityReport } = runtimeDetails;
    return dom.article(
      {
        className: "page js-runtime-page",
      },
      RuntimeInfo(runtimeDetails.info),
      this.renderRemoteRuntimeActions(),
      runtimeDetails.serviceWorkersAvailable ? null : ServiceWorkersWarning(),
      CompatibilityWarning({ compatibilityReport }),
      // show component to enable/disable extension debugging.
      isExtensionDebugSettingNeeded(type)
        ? this.renderExtensionDebugSetting()
        : null,
      isSupportedDebugTargetPane(type, DEBUG_TARGET_PANE.TEMPORARY_EXTENSION)
        ? TemporaryExtensionInstaller({
            dispatch,
            temporaryInstallError,
        }) : null,
      this.renderDebugTargetPane("Temporary Extensions",
                                 temporaryExtensions,
                                 TemporaryExtensionAction,
                                 TemporaryExtensionDetail,
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

      showProfilerDialog ? ProfilerDialog({ dispatch, runtimeDetails }) : null,
    );
  }
}

const mapStateToProps = state => {
  return {
    collapsibilities: state.ui.debugTargetCollapsibilities,
    installedExtensions: state.debugTargets.installedExtensions,
    otherWorkers: state.debugTargets.otherWorkers,
    runtimeDetails: getCurrentRuntimeDetails(state.runtimes),
    serviceWorkers: state.debugTargets.serviceWorkers,
    sharedWorkers: state.debugTargets.sharedWorkers,
    showProfilerDialog: state.ui.showProfilerDialog,
    tabs: state.debugTargets.tabs,
    temporaryExtensions: state.debugTargets.temporaryExtensions,
    temporaryInstallError: state.ui.temporaryInstallError,
  };
};

module.exports = connect(mapStateToProps)(RuntimePage);
