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

const FieldPair = createFactory(require("./FieldPair"));

const Types = require("../../types/index");

/**
 * This component displays detail information for worker.
 */
class WorkerDetail extends PureComponent {
  static get propTypes() {
    return {
      // Provided by wrapping the component with FluentReact.withLocalization.
      getString: PropTypes.func.isRequired,
      target: Types.debugTarget.isRequired,
    };
  }

  renderFetch() {
    const { fetch } = this.props.target.details;
    const status = fetch === SERVICE_WORKER_FETCH_STATES.LISTENING
                    ? "listening"
                    : "not-listening";

    return Localized(
      {
        id: "about-debugging-worker-fetch",
        attrs: { label: true, value: true },
        $status: status,
      },
      FieldPair(
        {
          slug: "fetch",
          label: "Fetch",
          value: status,
        }
      )
    );
  }

  renderScope() {
    const { scope } = this.props.target.details;

    return Localized(
      {
        id: "about-debugging-worker-scope",
        attrs: { label: true },
      },
      FieldPair(
        {
          slug: "scope",
          label: "Scope",
          value: scope,
        }
      ),
    );
  }

  renderStatus() {
    const status = this.props.target.details.status.toLowerCase();

    return FieldPair(
      {
        slug: "status",
        label: Localized(
          {
            id: "about-debugging-worker-status",
            $status: status,
          },
          dom.span(
            { className: `badge ${status === "running" ? "badge--success" : ""}`},
            status
          )
        ),
      }
    );
  }

  render() {
    const { fetch, scope, status } = this.props.target.details;

    return dom.dl(
      {
        className: "worker-detail",
      },
      fetch ? this.renderFetch() : null,
      scope ? this.renderScope() : null,
      status ? this.renderStatus() : null,
    );
  }
}

module.exports = FluentReact.withLocalization(WorkerDetail);
