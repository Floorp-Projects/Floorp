/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { connect } = require("devtools/client/shared/vendor/react-redux");

// Components
loader.lazyGetter(this, "MonitorPanel", function () {
  return createFactory(require("./MonitorPanel"));
});
loader.lazyGetter(this, "StatisticsPanel", function () {
  return createFactory(require("./StatisticsPanel"));
});

const { div } = dom;

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
