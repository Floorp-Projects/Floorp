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
  section,
} = require("devtools/client/shared/vendor/react-dom-factories");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const Types = require("devtools/client/application/src/types/index");
const RegistrationList = createFactory(
  require("devtools/client/application/src/components/service-workers/RegistrationList")
);
const RegistrationListEmpty = createFactory(
  require("devtools/client/application/src/components/service-workers/RegistrationListEmpty")
);

class WorkersPage extends PureComponent {
  static get propTypes() {
    return {
      // mapped from state
      canDebugWorkers: PropTypes.bool.isRequired,
      domain: PropTypes.string.isRequired,
      registrations: Types.registrationArray.isRequired,
    };
  }

  render() {
    const { canDebugWorkers, domain, registrations } = this.props;

    // Filter out workers from other domains
    const domainWorkers = registrations.filter(
      x => x.workers.length > 0 && new URL(x.workers[0].url).hostname === domain
    );
    const isListEmpty = domainWorkers.length === 0;

    return section(
      {
        className: `app-page js-service-workers-page ${
          isListEmpty ? "app-page--empty" : ""
        }`,
      },
      isListEmpty
        ? RegistrationListEmpty({})
        : RegistrationList({
            canDebugWorkers,
            registrations: domainWorkers,
          })
    );
  }
}

function mapStateToProps(state) {
  return {
    canDebugWorkers: state.workers.canDebugWorkers,
    domain: state.page.domain,
    registrations: state.workers.list,
  };
}

// Exports
module.exports = connect(mapStateToProps)(WorkersPage);
