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

const WorkerList = createFactory(require("./WorkerList"));
const WorkerListEmpty = createFactory(require("./WorkerListEmpty"));

class WorkersPage extends PureComponent {
  static get propTypes() {
    return {
      // mapped from state
      canDebugWorkers: PropTypes.bool.isRequired,
      domain: PropTypes.string.isRequired,
      workers: PropTypes.array.isRequired,
    };
  }

  render() {
    const { canDebugWorkers, domain, workers } = this.props;

    // Filter out workers from other domains
    const domainWorkers = workers.filter(
      x => new URL(x.url).hostname === domain
    );
    const isWorkerListEmpty = domainWorkers.length === 0;

    return section(
      {
        className: `app-page ${isWorkerListEmpty ? "app-page--empty" : ""}`,
      },
      isWorkerListEmpty
        ? WorkerListEmpty({})
        : WorkerList({
            canDebugWorkers,
            workers: domainWorkers,
          })
    );
  }
}

function mapStateToProps(state) {
  return {
    canDebugWorkers: state.workers.canDebugWorkers,
    domain: state.page.domain,
    workers: state.workers.list,
  };
}

// Exports
module.exports = connect(mapStateToProps)(WorkersPage);
