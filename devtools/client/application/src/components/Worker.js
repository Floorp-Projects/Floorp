/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { a, button, div, li, span } = require("devtools/client/shared/vendor/react-dom-factories");
const Services = require("Services");

loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/debugger-client", true);
loader.lazyRequireGetter(this, "gDevToolsBrowser",
  "devtools/client/framework/devtools-browser", true);

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

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

    let { client, worker } = this.props;
    gDevToolsBrowser.openWorkerToolbox(client, worker.workerActor);
  }

  start() {
    if (!this.isActive() || this.isRunning()) {
      console.log("Running or inactive service workers cannot be started");
      return;
    }

    let { client, worker } = this.props;
    client.request({
      to: worker.registrationActor,
      type: "start"
    });
  }

  unregister() {
    let { client, worker } = this.props;
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
    let [, remainder] = scope.split("://");
    return remainder || scope;
  }

  formatSource(source) {
    let parts = source.split("/");
    return parts[parts.length - 1];
  }

  render() {
    let { worker } = this.props;
    let status = this.getServiceWorkerStatus();

    const unregisterButton = this.isActive() ?
      button({
        onClick: this.unregister,
        className: "devtools-button unregister-button",
        "data-standalone": true
      },
        Strings.GetStringFromName("unregister"))
      : null;

    const debugLinkDisabled = this.isRunning() ? "" : "disabled";
    const debugLink = a({
      onClick: this.isRunning() ? this.debug : null,
      title: this.isRunning() ? null : "Only running service workers can be debugged",
      className: `${debugLinkDisabled} debug-link`
    },
      Strings.GetStringFromName("debug"));

    const startLink = !this.isRunning() ?
      a({ onClick: this.start, className: "start-link" },
        Strings.GetStringFromName("start"))
      : null;

    return li({ className: "service-worker-container" },
      div(
        { className: "service-worker-scope" },
        span({ title: worker.scope }, this.formatScope(worker.scope)),
        unregisterButton),
      div(
        { className: "service-worker-source" },
        span({ className: "service-worker-meta-name" }, "Source"),
        span({ title: worker.scope }, this.formatSource(worker.url)),
        debugLink),
      div(
        { className: `service-worker-status service-worker-status-${status}` },
        span({ className: "service-worker-meta-name" }, "Status"),
        Strings.GetStringFromName(status).toLowerCase(),
        startLink)
    );
  }
}

module.exports = Worker;
