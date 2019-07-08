/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  createFactory,
  Component,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { main } = require("devtools/client/shared/vendor/react-dom-factories");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const LocalizationProvider = createFactory(FluentReact.LocalizationProvider);

const WorkerList = createFactory(require("./WorkerList"));
const WorkerListEmpty = createFactory(require("./WorkerListEmpty"));

/**
 * This is the main component for the application panel.
 */
class App extends Component {
  static get propTypes() {
    return {
      // mapped from state
      canDebugWorkers: PropTypes.bool.isRequired,
      client: PropTypes.object.isRequired,
      // mapped from state
      domain: PropTypes.string.isRequired,
      fluentBundles: PropTypes.array.isRequired,
      serviceContainer: PropTypes.object.isRequired,
      // mapped from state
      workers: PropTypes.array.isRequired,
    };
  }

  render() {
    const {
      canDebugWorkers,
      client,
      domain,
      fluentBundles,
      serviceContainer,
      workers,
    } = this.props;

    // Filter out workers from other domains
    const domainWorkers = workers.filter(
      x => new URL(x.url).hostname === domain
    );
    const isWorkerListEmpty = domainWorkers.length === 0;

    return LocalizationProvider(
      { messages: fluentBundles },
      main(
        {
          className: `application ${
            isWorkerListEmpty ? "application--empty" : ""
          }`,
        },
        isWorkerListEmpty
          ? WorkerListEmpty({ serviceContainer })
          : WorkerList({ canDebugWorkers, client, workers: domainWorkers })
      )
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

module.exports = connect(mapStateToProps)(App);
