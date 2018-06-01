/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { a, br, button, dd, dl, dt, header, li, section, span, time } =
  require("devtools/client/shared/vendor/react-dom-factories");
const { getUnicodeUrl, getUnicodeUrlPath } = require("devtools/client/shared/unicode-url");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/debugger-client", true);
loader.lazyRequireGetter(this, "gDevToolsBrowser",
  "devtools/client/framework/devtools-browser", true);

/**
 * This component is dedicated to display a worker, more accurately a service worker, in
 * the list of workers displayed in the application panel. It displays information about
 * the worker as well as action links and buttons to interact with the worker (e.g. debug,
 * unregister, update etc...).
 */
class Worker extends Component {
  static get propTypes() {
    return {
      client: PropTypes.instanceOf(DebuggerClient).isRequired,
      debugDisabled: PropTypes.bool,
      worker: PropTypes.shape({
        active: PropTypes.bool,
        name: PropTypes.string.isRequired,
        scope: PropTypes.string.isRequired,
        // registrationActor can be missing in e10s.
        registrationActor: PropTypes.string,
        workerActor: PropTypes.string
      }).isRequired
    };
  }

  constructor(props) {
    super(props);

    this.debug = this.debug.bind(this);
    this.start = this.start.bind(this);
    this.unregister = this.unregister.bind(this);
  }

  debug() {
    if (!this.isRunning()) {
      console.log("Service workers cannot be debugged if they are not running");
      return;
    }

    const { client, worker } = this.props;
    gDevToolsBrowser.openWorkerToolbox(client, worker.workerActor);
  }

  start() {
    if (!this.isActive() || this.isRunning()) {
      console.log("Running or inactive service workers cannot be started");
      return;
    }

    const { client, worker } = this.props;
    client.request({
      to: worker.registrationActor,
      type: "start"
    });
  }

  unregister() {
    const { client, worker } = this.props;
    client.request({
      to: worker.registrationActor,
      type: "unregister"
    });
  }

  isRunning() {
    // We know the worker is running if it has a worker actor.
    return !!this.props.worker.workerActor;
  }

  isActive() {
    return this.props.worker.active;
  }

  getServiceWorkerStatus() {
    if (this.isActive() && this.isRunning()) {
      return "running";
    } else if (this.isActive()) {
      return "stopped";
    }
    // We cannot get service worker registrations unless the registration is in
    // ACTIVE state. Unable to know the actual state ("installing", "waiting"), we
    // display a custom state "registering" for now. See Bug 1153292.
    return "registering";
  }

  formatScope(scope) {
    const [, remainder] = getUnicodeUrl(scope).split("://");
    return remainder || scope;
  }

  formatSource(source) {
    const parts = source.split("/");
    return getUnicodeUrlPath(parts[parts.length - 1]);
  }

  render() {
    const { worker } = this.props;
    const status = this.getServiceWorkerStatus();

    const unregisterButton = this.isActive() ?
      Localized(
        { id: "serviceworker-worker-unregister" },
        button({
          onClick: this.unregister,
          className: "devtools-button worker__unregister-button js-unregister-button",
          "data-standalone": true
        })
      ) : null;

    const debugLinkDisabled = this.isRunning() ? "" : "disabled";

    const debugLink = Localized({
      id: "serviceworker-worker-debug",
      // The localized title is only displayed if the debug link is disabled.
      attrs: { title: !this.isRunning() }
    },
      a({
        onClick: this.isRunning() ? this.debug : null,
        className: `${debugLinkDisabled} worker__debug-link js-debug-link`
      })
    );

    const startLink = !this.isRunning() ?
      Localized(
        { id: "serviceworker-worker-start" },
        a({
          onClick: this.start,
          className: "worker__start-link"
        })
      ) : null;

    const lastUpdated = worker.lastUpdateTime ?
      Localized(
        {
          id: "serviceworker-worker-updated",
          // XXX: $date should normally be a Date object, but we pass the timestamp as a
          // workaround. See Bug 1465718. worker.lastUpdateTime is in microseconds,
          // convert to a valid timestamp in milliseconds by dividing by 1000.
          "$date": worker.lastUpdateTime / 1000,
          time: time({ className: "js-sw-updated" })
        },
        span({ className: "worker__data__updated" })
      ) : null;

    return li({ className: "worker js-sw-container" },
      header(
        { className: "worker__header" },
        span({ title: worker.scope, className: "worker__scope js-sw-scope" },
          this.formatScope(worker.scope)),
        section(
          { className: "worker__controls" },
          unregisterButton),
      ),
      dl(
        { className: "worker__data" },
        Localized({ id: "serviceworker-worker-source" },
          dt({ className: "worker__meta-name" })
        ),
        dd({},
            span({ title: worker.scope, className: "worker__source-url js-source-url" },
              this.formatSource(worker.url)),
            debugLink,
            lastUpdated ? br({}) : null,
            lastUpdated ? lastUpdated : null),
        Localized({ id: "serviceworker-worker-status" },
          dt({ className: "worker__meta-name" })
        ),
        dd({},
          Localized(
            { id: "serviceworker-worker-status-" + status },
            span({}),
          ),
          startLink
        )
      )
    );
  }
}

module.exports = Worker;
