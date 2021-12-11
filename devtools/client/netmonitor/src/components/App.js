/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");

// Components
loader.lazyGetter(this, "AppErrorBoundary", function() {
  return createFactory(
    require("devtools/client/shared/components/AppErrorBoundary")
  );
});
loader.lazyGetter(this, "MonitorPanel", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/MonitorPanel")
  );
});
loader.lazyGetter(this, "StatisticsPanel", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/StatisticsPanel")
  );
});
loader.lazyGetter(this, "DropHarHandler", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/DropHarHandler")
  );
});

// Localized strings for (devtools/client/locales/en-US/startup.properties)
loader.lazyGetter(this, "L10N", function() {
  const { LocalizationHelper } = require("devtools/shared/l10n");
  return new LocalizationHelper("devtools/client/locales/startup.properties");
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
      sourceMapURLService: PropTypes.object,
      // True if the stats panel is opened.
      statisticsOpen: PropTypes.bool.isRequired,
      // Document which settings menu will be injected to
      toolboxDoc: PropTypes.object.isRequired,
      // Syncing blocked requests
      addBlockedUrl: PropTypes.func,
    };
  }
  // Rendering

  render() {
    const {
      actions,
      connector,
      openLink,
      openSplitConsole,
      sourceMapURLService,
      statisticsOpen,
      toolboxDoc,
    } = this.props;

    return div(
      { className: "network-monitor" },
      AppErrorBoundary(
        {
          className: "app-error-boundary",
          panel: L10N.getStr("netmonitor.label"),
        },
        !statisticsOpen
          ? DropHarHandler(
              {
                actions,
                openSplitConsole,
              },
              MonitorPanel({
                actions,
                connector,
                openSplitConsole,
                sourceMapURLService,
                openLink,
                toolboxDoc,
              })
            )
          : StatisticsPanel({
              connector,
            })
      )
    );
  }
}

// Exports

module.exports = connect(state => ({
  statisticsOpen: state.ui.statisticsOpen,
}))(App);
