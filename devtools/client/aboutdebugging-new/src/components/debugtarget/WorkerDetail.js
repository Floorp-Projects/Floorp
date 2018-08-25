/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

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
    };
  }

  renderFetch() {
    const { fetch } = this.props.target.details;
    const label = fetch === SERVICE_WORKER_FETCH_STATES.LISTENING
                    ? "Listening for fetch events"
                    : "Not listening for fetch events";
    return this.renderField("Fetch", label);
  }

  renderField(name, value) {
    return [
      dom.dt({}, name),
      dom.dd(
        {
          className: "ellipsis-text",
          title: value,
        },
        value,
      ),
    ];
  }

  renderStatus() {
    const status = this.props.target.details.status.toLowerCase();

    return dom.div(
      {
        className: `worker-detail__status worker-detail__status--${ status }`,
      },
      status
    );
  }

  render() {
    const { fetch, scope, status } = this.props.target.details;

    return dom.dl(
      {
        className: "worker-detail",
      },
      fetch ? this.renderFetch() : null,
      scope ? this.renderField("Scope", scope) : null,
      status ? this.renderStatus() : null,
    );
  }
}

module.exports = WorkerDetail;
