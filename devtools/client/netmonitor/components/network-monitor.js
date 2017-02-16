/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");

// Components
const MonitoPanel = createFactory(require("./monitor-panel"));
const StatisticsPanel = createFactory(require("./statistics-panel"));

const { div } = DOM;

/*
 * Network monitor component
 */
function NetworkMonitor({ statisticsOpen }) {
  return (
    div({ className: "network-monitor" },
      !statisticsOpen ? MonitoPanel() : StatisticsPanel()
    )
  );
}

NetworkMonitor.displayName = "NetworkMonitor";

NetworkMonitor.propTypes = {
  statisticsOpen: PropTypes.bool.isRequired,
};

module.exports = connect(
  (state) => ({ statisticsOpen: state.ui.statisticsOpen }),
)(NetworkMonitor);
