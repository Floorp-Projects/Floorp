/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { connect } = require("devtools/client/shared/redux/visibility-handler-connect");

// Components
loader.lazyGetter(this, "MonitorPanel", function() {
  return createFactory(require("./MonitorPanel"));
});
loader.lazyGetter(this, "StatisticsPanel", function() {
  return createFactory(require("./StatisticsPanel"));
});
loader.lazyGetter(this, "DropHarHandler", function() {
  return createFactory(require("./DropHarHandler"));
});

const { div } = dom;

/**
 * App component
 * The top level component for representing main panel
 */
class App extends Component {
  static get propTypes() {
    return {
      // List of actions passed to HAR importer.
      actions: PropTypes.object.isRequired,
      // The backend connector object.
      connector: PropTypes.object.isRequired,
      // Callback for opening links in the UI
      openLink: PropTypes.func,
      // Callback for opening split console.
      openSplitConsole: PropTypes.func,
      // Service to enable the source map feature.
      sourceMapService: PropTypes.object,
      // True if the stats panel is opened.
      statisticsOpen: PropTypes.bool.isRequired,
    };
  }

  // Rendering

  render() {
    const {
      actions,
      connector,
      openLink,
      openSplitConsole,
      sourceMapService,
      statisticsOpen,
    } = this.props;

    return (
      div({className: "network-monitor"},
        !statisticsOpen ?
          DropHarHandler({
            actions,
            openSplitConsole,
          },
            MonitorPanel({
              actions,
              connector,
              openSplitConsole,
              sourceMapService,
              openLink,
            })
          ) : StatisticsPanel({
            connector
          }),
      )
    );
  }
}

// Exports

module.exports = connect(
  (state) => ({ statisticsOpen: state.ui.statisticsOpen }),
)(App);
