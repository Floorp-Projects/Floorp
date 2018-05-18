/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { createFactory, Component } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { main } = require("devtools/client/shared/vendor/react-dom-factories");

const WorkerList = createFactory(require("./WorkerList"));
const WorkerListEmpty = createFactory(require("./WorkerListEmpty"));

/**
 * This is the main component for the application panel.
 */
class App extends Component {
  static get propTypes() {
    return {
      client: PropTypes.object.isRequired,
      workers: PropTypes.object.isRequired,
      serviceContainer: PropTypes.object.isRequired,
      domain: PropTypes.string.isRequired,
    };
  }

  render() {
    let { workers, domain, client, serviceContainer } = this.props;

    // Filter out workers from other domains
    workers = workers.filter((x) => new URL(x.url).hostname === domain);
    const isEmpty = workers.length === 0;

    return (
      main(
        { className: `application ${isEmpty ? "application--empty" : ""}` },
        isEmpty
          ? WorkerListEmpty({ serviceContainer })
          : WorkerList({ workers, client, serviceContainer })
      )
    );
  }
}

// Exports

module.exports = connect(
  (state) => ({ workers: state.workers.list, domain: state.page.domain }),
)(App);
