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

/**
 * App component
 * The top level component for representing main panel
 */
function App({
  connector,
  openLink,
  sourceMapService,
  statisticsOpen,
}) {
  return (
    div({ className: "network-monitor" },
      !statisticsOpen ? MonitorPanel({
        connector,
        sourceMapService,
        openLink,
      }) : StatisticsPanel({
        connector
      })
    )
  );
}

App.displayName = "App";

App.propTypes = {
  // The backend connector object.
  connector: PropTypes.object.isRequired,
  // Callback for opening links in the UI
  openLink: PropTypes.func,
  // Service to enable the source map feature.
  sourceMapService: PropTypes.object,
  // True if the stats panel is opened.
  statisticsOpen: PropTypes.bool.isRequired,
};

module.exports = connect(
  (state) => ({ statisticsOpen: state.ui.statisticsOpen }),
)(App);
