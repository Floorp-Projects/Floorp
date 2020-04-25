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
  dd,
  dl,
  dt,
  section,
  span,
} = require("devtools/client/shared/vendor/react-dom-factories");

const { getUnicodeUrlPath } = require("devtools/client/shared/unicode-url");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);
const { l10n } = require("devtools/client/application/src/modules/l10n");

const {
  services,
} = require("devtools/client/application/src/modules/application-services");
const Types = require("devtools/client/application/src/types/index");

const UIButton = createFactory(
  require("devtools/client/application/src/components/ui/UIButton")
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
      worker: PropTypes.shape(Types.worker).isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.debug = this.debug.bind(this);
    this.start = this.start.bind(this);
  }

  debug() {
    if (!this.isRunning()) {
      console.log("Service workers cannot be debugged if they are not running");
      return;
    }

    services.openWorkerInDebugger(this.props.worker.workerTargetFront);
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

  isRunning() {
    // We know the worker is running if it has a worker actor.
    return !!this.props.worker.workerTargetFront;
  }

  isActive() {
    return this.props.worker.isActive;
  }

  getLocalizedStatus() {
    if (this.isActive() && this.isRunning()) {
      return l10n.getString("serviceworker-worker-status-running");
    } else if (this.isActive()) {
      return l10n.getString("serviceworker-worker-status-stopped");
    }
    // NOTE: this is already localized by the service worker front
    // (strings are in debugger.properties)
    return this.props.worker.stateText;
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

    // avoid rendering the button at all for workers that are either running,
    // or in a state that prevents them from starting (like waiting)
    if (this.isRunning() || !this.isActive()) {
      return null;
    }

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
    const statusText = this.getLocalizedStatus();

    return section(
      { className: "worker js-sw-worker" },
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
              title: worker.url,
              className: "worker__source-url js-source-url",
            },
            this.formatSource(worker.url)
          ),
          " ",
          this.renderDebugButton()
        ),
        Localized(
          { id: "serviceworker-worker-status" },
          dt({ className: "worker__meta-name" })
        ),
        dd(
          {},
          span({ className: "js-worker-status worker__status" }, statusText),
          " ",
          this.renderStartButton()
        )
      )
    );
  }
}

module.exports = Worker;
