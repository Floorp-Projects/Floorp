/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const {
  br,
  dd,
  dl,
  dt,
  header,
  li,
  section,
  span,
  time,
} = require("devtools/client/shared/vendor/react-dom-factories");

const {
  getUnicodeUrl,
  getUnicodeUrlPath,
} = require("devtools/client/shared/unicode-url");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const UIButton = createFactory(require("../ui/UIButton"));

loader.lazyRequireGetter(
  this,
  "gDevToolsBrowser",
  "devtools/client/framework/devtools-browser",
  true
);

/**
 * This component is dedicated to display a worker, more accurately a service worker, in
 * the list of workers displayed in the application panel. It displays information about
 * the worker as well as action links and buttons to interact with the worker (e.g. debug,
 * unregister, update etc...).
 */
class Worker extends PureComponent {
  static get propTypes() {
    return {
      isDebugEnabled: PropTypes.bool.isRequired,
      worker: PropTypes.shape({
        active: PropTypes.bool,
        name: PropTypes.string.isRequired,
        scope: PropTypes.string.isRequired,
        lastUpdateTime: PropTypes.number.isRequired,
        url: PropTypes.string.isRequired,
        // registrationFront can be missing in e10s.
        registrationFront: PropTypes.object,
        workerTargetFront: PropTypes.object,
      }).isRequired,
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

    const { workerTargetFront } = this.props.worker;
    gDevToolsBrowser.openWorkerToolbox(workerTargetFront);
  }

  start() {
    if (!this.props.isDebugEnabled) {
      console.log("Service workers cannot be started in multi-e10s");
      return;
    }

    if (!this.isActive() || this.isRunning()) {
      console.log("Running or inactive service workers cannot be started");
      return;
    }

    const { registrationFront } = this.props.worker;
    registrationFront.start();
  }

  unregister() {
    const { registrationFront } = this.props.worker;
    registrationFront.unregister();
  }

  isRunning() {
    // We know the worker is running if it has a worker actor.
    return !!this.props.worker.workerTargetFront;
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

  renderDebugButton() {
    const { isDebugEnabled } = this.props;

    const isDisabled = !this.isRunning() || !isDebugEnabled;

    const localizationId = isDebugEnabled
      ? "serviceworker-worker-debug"
      : "serviceworker-worker-debug-forbidden";

    return Localized(
      {
        id: localizationId,
        // The localized title is only displayed if the debug link is disabled.
        attrs: {
          title: isDisabled,
        },
      },
      UIButton({
        onClick: this.debug,
        className: `js-debug-button`,
        disabled: isDisabled,
        size: "micro",
      })
    );
  }

  renderStartButton() {
    const { isDebugEnabled } = this.props;
    const isDisabled = !isDebugEnabled;

    return Localized(
      {
        id: "serviceworker-worker-start2",
        // The localized title is only displayed if the debug link is disabled.
        attrs: {
          title: !isDisabled,
        },
      },
      UIButton({
        onClick: this.start,
        className: `js-start-button`,
        disabled: isDisabled,
        size: "micro",
      })
    );
  }

  render() {
    const { worker } = this.props;
    const status = this.getServiceWorkerStatus();

    const unregisterButton = this.isActive()
      ? Localized(
          { id: "serviceworker-worker-unregister" },
          UIButton({
            onClick: this.unregister,
            className: "worker__unregister-button js-unregister-button",
          })
        )
      : null;

    const lastUpdated = worker.lastUpdateTime
      ? Localized(
          {
            id: "serviceworker-worker-updated",
            // XXX: $date should normally be a Date object, but we pass the timestamp as a
            // workaround. See Bug 1465718. worker.lastUpdateTime is in microseconds,
            // convert to a valid timestamp in milliseconds by dividing by 1000.
            $date: worker.lastUpdateTime / 1000,
            time: time({ className: "js-sw-updated" }),
          },
          span({ className: "worker__data__updated" })
        )
      : null;

    return li(
      { className: "worker js-sw-container" },
      header(
        { className: "worker__header" },
        span(
          { title: worker.scope, className: "worker__scope js-sw-scope" },
          this.formatScope(worker.scope)
        ),
        section({ className: "worker__controls" }, unregisterButton)
      ),
      dl(
        { className: "worker__data" },
        Localized(
          { id: "serviceworker-worker-source" },
          dt({ className: "worker__meta-name" })
        ),
        dd(
          {},
          span(
            {
              title: worker.scope,
              className: "worker__source-url js-source-url",
            },
            this.formatSource(worker.url)
          ),
          " ",
          this.renderDebugButton(),
          lastUpdated ? br({}) : null,
          lastUpdated ? lastUpdated : null
        ),
        Localized(
          { id: "serviceworker-worker-status" },
          dt({ className: "worker__meta-name" })
        ),
        dd(
          {},
          Localized(
            { id: "serviceworker-worker-status-" + status },
            span({ className: "js-worker-status" })
          ),
          " ",
          !this.isRunning() ? this.renderStartButton() : null
        )
      )
    );
  }
}

module.exports = Worker;
