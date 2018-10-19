/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const {
  SERVICE_WORKER_FETCH_STATES,
} = require("../../constants");

/**
 * This component displays detail information for worker.
 */
class WorkerDetail extends PureComponent {
  static get propTypes() {
    return {
      target: PropTypes.object.isRequired,
      // Provided by wrapping the component with FluentReact.withLocalization.
      getString: PropTypes.func.isRequired,
    };
  }

  getStatusFtlId(status) {
    switch (status) {
      case "running":
        return "about-debugging-worker-status-running";
      case "stopped":
        return "about-debugging-worker-status-stopped";
      case "registering":
        return "about-debugging-worker-status-registering";
      default:
        // Provided with a null id, Localized will simply use the fallback value defined
        // in the component.
        return null;
    }
  }

  renderFetch() {
    const { fetch } = this.props.target.details;
    const name = this.props.getString("about-debugging-worker-fetch");
    const label = fetch === SERVICE_WORKER_FETCH_STATES.LISTENING
                    ? this.props.getString("about-debugging-worker-fetch-listening")
                    : this.props.getString("about-debugging-worker-fetch-not-listening");
    return this.renderField("fetch", name, label);
  }

  renderField(key, name, value) {
    return [
      dom.dt({ key: `${ key }-dt` }, name),
      dom.dd(
        {
          className: "ellipsis-text",
          key: `${ key }-dd`,
          title: value,
        },
        value,
      ),
    ];
  }

  renderStatus() {
    const status = this.props.target.details.status.toLowerCase();
    const ftlId = this.getStatusFtlId(status);

    return Localized(
      {
        id: ftlId,
      },
      dom.div(
        {
          className: `worker-detail__status worker-detail__status--${ status }`,
        },
        status
      )
    );
  }

  render() {
    const { fetch, scope, status } = this.props.target.details;

    return dom.dl(
      {
        className: "worker-detail",
      },
      fetch ? this.renderFetch() : null,
      scope ? this.renderField("scope", "Scope", scope) : null,
      status ? this.renderStatus() : null,
    );
  }
}

module.exports = FluentReact.withLocalization(WorkerDetail);
