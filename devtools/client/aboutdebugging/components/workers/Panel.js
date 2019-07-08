/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals window */
"use strict";

loader.lazyImporter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const Services = require("Services");
const {
  addMultiE10sListener,
  isMultiE10s,
  removeMultiE10sListener,
} = require("devtools/shared/multi-e10s-helper");

const PanelHeader = createFactory(require("../PanelHeader"));
const TargetList = createFactory(require("../TargetList"));
const WorkerTarget = createFactory(require("./Target"));
const MultiE10SWarning = createFactory(require("./MultiE10sWarning"));
const ServiceWorkerTarget = createFactory(require("./ServiceWorkerTarget"));

loader.lazyImporter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

loader.lazyRequireGetter(
  this,
  "DebuggerClient",
  "devtools/shared/client/debugger-client",
  true
);

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties"
);

const WorkerIcon = "chrome://devtools/skin/images/debugging-workers.svg";
const MORE_INFO_URL =
  "https://developer.mozilla.org/en-US/docs/Tools/about%3Adebugging" +
  "#Service_workers_not_compatible";

class WorkersPanel extends Component {
  static get propTypes() {
    return {
      client: PropTypes.instanceOf(DebuggerClient).isRequired,
      id: PropTypes.string.isRequired,
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
    // When calling RootFront.listAllWorkers, ContentProcessTargetActor are created
    // for each content process, which sends `workerListChanged` events.
    client.mainRoot.onFront("contentProcessTarget", front => {
      front.on("workerListChanged", this.updateWorkers);
      this.state.contentProcessFronts.push(front);
    });
    client.mainRoot.onFront("serviceWorkerRegistration", front => {
      this.state.serviceWorkerRegistrationFronts.push(front);
      front.on("push-subscription-modified", this.updateWorkers);
      front.on("registration-changed", this.updateWorkers);
    });
    client.mainRoot.on("workerListChanged", this.updateWorkers);

    client.mainRoot.on(
      "serviceWorkerRegistrationListChanged",
      this.updateWorkers
    );
    client.mainRoot.on("processListChanged", this.updateWorkers);

    addMultiE10sListener(this.updateMultiE10S);

    this.updateMultiE10S();
    this.updateWorkers();
  }

  componentWillUnmount() {
    const client = this.props.client;
    client.mainRoot.off("processListChanged", this.updateWorkers);
    client.mainRoot.off(
      "serviceWorkerRegistrationListChanged",
      this.updateWorkers
    );
    client.mainRoot.off("workerListChanged", this.updateWorkers);
    for (const front of this.state.contentProcessFronts) {
      front.off("workerListChanged", this.updateWorkers);
    }
    for (const front of this.state.serviceWorkerRegistrationFronts) {
      front.off("push-subscription-modified", this.updateWorkers);
      front.off("registration-changed", this.updateWorkers);
    }

    removeMultiE10sListener(this.updateMultiE10S);
  }

  get initialState() {
    return {
      workers: {
        service: [],
        shared: [],
        other: [],
      },
      isMultiE10S: isMultiE10s(),

      // List of ContentProcessTargetFront registered from componentWillMount
      // from which we listen for worker list changes
      contentProcessFronts: [],
      serviceWorkerRegistrationFronts: [],
    };
  }

  updateMultiE10S() {
    this.setState({ isMultiE10S: isMultiE10s() });
  }

  updateWorkers() {
    const workers = this.initialState.workers;

    this.props.client.mainRoot
      .listAllWorkers()
      .then(({ service, other, shared }) => {
        workers.service = service.map(f =>
          Object.assign({ icon: WorkerIcon }, f)
        );
        workers.other = other.map(f => Object.assign({ icon: WorkerIcon }, f));
        workers.shared = shared.map(f =>
          Object.assign({ icon: WorkerIcon }, f)
        );

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
    const isServiceWorkerDisabled = !Services.prefs.getBoolPref(
      "dom.serviceWorkers.enabled"
    );

    const isDisabled =
      isWindowPrivate || isPrivateBrowsingMode || isServiceWorkerDisabled;
    if (!isDisabled) {
      return "";
    }
    return dom.p(
      {
        className: "service-worker-disabled",
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
          target: "_blank",
        },
        Strings.GetStringFromName("configurationIsNotCompatible.learnMore")
      )
    );
  }

  render() {
    const { client, id } = this.props;
    const { workers, isMultiE10S } = this.state;

    return dom.div(
      {
        id: id + "-panel",
        className: "panel",
        role: "tabpanel",
        "aria-labelledby": id + "-header",
      },
      PanelHeader({
        id: id + "-header",
        name: Strings.GetStringFromName("workers"),
      }),
      isMultiE10S ? MultiE10SWarning() : "",
      dom.div(
        {
          id: "workers",
          className: "inverted-icons",
        },
        TargetList({
          client,
          debugDisabled: isMultiE10S,
          error: this.renderServiceWorkersError(),
          id: "service-workers",
          name: Strings.GetStringFromName("serviceWorkers"),
          sort: true,
          targetClass: ServiceWorkerTarget,
          targets: workers.service,
        }),
        TargetList({
          client,
          id: "shared-workers",
          name: Strings.GetStringFromName("sharedWorkers"),
          sort: true,
          targetClass: WorkerTarget,
          targets: workers.shared,
        }),
        TargetList({
          client,
          id: "other-workers",
          name: Strings.GetStringFromName("otherWorkers"),
          sort: true,
          targetClass: WorkerTarget,
          targets: workers.other,
        })
      )
    );
  }
}

module.exports = WorkersPanel;
