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
const MonitorPanel = createFactory(require("./monitor-panel"));
const StatisticsPanel = createFactory(require("./statistics-panel"));

const { div } = DOM;

/*
 * App component
 * The top level component for representing main panel
 */
function App({
  openLink,
  sourceMapService,
  statisticsOpen,
}) {
  return (
    div({ className: "network-monitor" },
      !statisticsOpen ? MonitorPanel({ openLink, sourceMapService }) : StatisticsPanel()
    )
  );
}

App.displayName = "App";

App.propTypes = {
  openLink: PropTypes.func,
  // Service to enable the source map feature.
  sourceMapService: PropTypes.object,
  statisticsOpen: PropTypes.bool.isRequired,
};

module.exports = connect(
  (state) => ({ statisticsOpen: state.ui.statisticsOpen }),
)(App);
