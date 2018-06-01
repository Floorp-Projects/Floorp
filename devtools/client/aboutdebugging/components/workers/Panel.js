/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals window */
"use strict";

loader.lazyImporter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const Services = require("Services");

const PanelHeader = createFactory(require("../PanelHeader"));
const TargetList = createFactory(require("../TargetList"));
const WorkerTarget = createFactory(require("./Target"));
const MultiE10SWarning = createFactory(require("./MultiE10sWarning"));
const ServiceWorkerTarget = createFactory(require("./ServiceWorkerTarget"));

loader.lazyImporter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");

loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/debugger-client", true);

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

const WorkerIcon = "chrome://devtools/skin/images/debugging-workers.svg";
const MORE_INFO_URL = "https://developer.mozilla.org/en-US/docs/Tools/about%3Adebugging" +
                      "#Service_workers_not_compatible";
const PROCESS_COUNT_PREF = "dom.ipc.processCount";
const MULTI_OPTOUT_PREF = "dom.ipc.multiOptOut";

class WorkersPanel extends Component {
  static get propTypes() {
    return {
      client: PropTypes.instanceOf(DebuggerClient).isRequired,
      id: PropTypes.string.isRequired
    };
  }

  constructor(props) {
    super(props);

    this.updateMultiE10S = this.updateMultiE10S.bind(this);
    this.updateWorkers = this.updateWorkers.bind(this);
    this.isE10S = this.isE10S.bind(this);
    this.renderServiceWorkersError = this.renderServiceWorkersError.bind(this);

    this.state = this.initialState;
  }

  componentDidMount() {
    const client = this.props.client;
    client.addListener("workerListChanged", this.updateWorkers);
    client.addListener("serviceWorkerRegistrationListChanged", this.updateWorkers);
    client.addListener("processListChanged", this.updateWorkers);
    client.addListener("registration-changed", this.updateWorkers);

    // Some notes about these observers:
    // - nsIPrefBranch.addObserver observes prefixes. In reality, watching
    //   PROCESS_COUNT_PREF watches two separate prefs:
    //   dom.ipc.processCount *and* dom.ipc.processCount.web. Because these
    //   are the two ways that we control the number of content processes,
    //   that works perfectly fine.
    // - The user might opt in or out of multi by setting the multi opt out
    //   pref. That affects whether we need to show our warning, so we need to
    //   update our state when that pref changes.
    // - In all cases, we don't have to manually check which pref changed to
    //   what. The platform code in nsIXULRuntime.maxWebProcessCount does all
    //   of that for us.
    Services.prefs.addObserver(PROCESS_COUNT_PREF, this.updateMultiE10S);
    Services.prefs.addObserver(MULTI_OPTOUT_PREF, this.updateMultiE10S);

    this.updateMultiE10S();
    this.updateWorkers();
  }

  componentWillUnmount() {
    const client = this.props.client;
    client.removeListener("processListChanged", this.updateWorkers);
    client.removeListener("serviceWorkerRegistrationListChanged", this.updateWorkers);
    client.removeListener("workerListChanged", this.updateWorkers);
    client.removeListener("registration-changed", this.updateWorkers);

    Services.prefs.removeObserver(PROCESS_COUNT_PREF, this.updateMultiE10S);
    Services.prefs.removeObserver(MULTI_OPTOUT_PREF, this.updateMultiE10S);
  }

  get initialState() {
    return {
      workers: {
        service: [],
        shared: [],
        other: []
      },
      processCount: 1,
    };
  }

  updateMultiE10S() {
    // We watch the pref but set the state based on
    // nsIXULRuntime.maxWebProcessCount.
    const processCount = Services.appinfo.maxWebProcessCount;
    this.setState({ processCount });
  }

  updateWorkers() {
    const workers = this.initialState.workers;

    this.props.client.mainRoot.listAllWorkers().then(({service, other, shared}) => {
      workers.service = service.map(f => Object.assign({ icon: WorkerIcon }, f));
      workers.other = other.map(f => Object.assign({ icon: WorkerIcon }, f));
      workers.shared = shared.map(f => Object.assign({ icon: WorkerIcon }, f));

      // XXX: Filter out the service worker registrations for which we couldn't
      // find the scriptSpec.
      workers.service = workers.service.filter(reg => !!reg.url);

      this.setState({ workers });
    });
  }

  isE10S() {
    return Services.appinfo.browserTabsRemoteAutostart;
  }

  renderServiceWorkersError() {
    const isWindowPrivate = PrivateBrowsingUtils.isContentWindowPrivate(window);
    const isPrivateBrowsingMode = PrivateBrowsingUtils.permanentPrivateBrowsing;
    const isServiceWorkerDisabled = !Services.prefs
                                    .getBoolPref("dom.serviceWorkers.enabled");

    const isDisabled =
      isWindowPrivate || isPrivateBrowsingMode || isServiceWorkerDisabled;
    if (!isDisabled) {
      return "";
    }
    return dom.p(
      {
        className: "service-worker-disabled"
      },
      dom.div({ className: "warning" }),
      dom.span(
        {
          className: "service-worker-disabled-label",
        },
        Strings.GetStringFromName("configurationIsNotCompatible.label")
      ),
      dom.a(
        {
          href: MORE_INFO_URL,
          target: "_blank"
        },
        Strings.GetStringFromName("configurationIsNotCompatible.learnMore")
      ),
    );
  }

  render() {
    const { client, id } = this.props;
    const { workers, processCount } = this.state;

    const isE10S = Services.appinfo.browserTabsRemoteAutostart;
    const isMultiE10S = isE10S && processCount > 1;

    return dom.div(
      {
        id: id + "-panel",
        className: "panel",
        role: "tabpanel",
        "aria-labelledby": id + "-header"
      },
      PanelHeader({
        id: id + "-header",
        name: Strings.GetStringFromName("workers")
      }),
      isMultiE10S ? MultiE10SWarning() : "",
      dom.div(
        {
          id: "workers",
          className: "inverted-icons"
        },
        TargetList({
          client,
          debugDisabled: isMultiE10S,
          error: this.renderServiceWorkersError(),
          id: "service-workers",
          name: Strings.GetStringFromName("serviceWorkers"),
          sort: true,
          targetClass: ServiceWorkerTarget,
          targets: workers.service
        }),
        TargetList({
          client,
          id: "shared-workers",
          name: Strings.GetStringFromName("sharedWorkers"),
          sort: true,
          targetClass: WorkerTarget,
          targets: workers.shared
        }),
        TargetList({
          client,
          id: "other-workers",
          name: Strings.GetStringFromName("otherWorkers"),
          sort: true,
          targetClass: WorkerTarget,
          targets: workers.other
        })
      )
    );
  }
}

module.exports = WorkersPanel;
