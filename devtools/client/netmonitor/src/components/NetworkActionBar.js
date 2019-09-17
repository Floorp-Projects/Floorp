/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const { div } = require("devtools/client/shared/vendor/react-dom-factories");
const { L10N } = require("../utils/l10n");

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Tabbar = createFactory(
  require("devtools/client/shared/components/tabs/TabBar")
);
const TabPanel = createFactory(
  require("devtools/client/shared/components/tabs/Tabs").TabPanel
);

loader.lazyGetter(this, "SearchPanel", function() {
  return createFactory(require("./search/SearchPanel"));
});

loader.lazyGetter(this, "RequestBlockingPanel", function() {
  return createFactory(require("./RequestBlockingPanel"));
});

class NetworkActionBar extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
    };
  }

  render() {
    const { connector } = this.props;

    // The request blocking and search panels are available behind a pref
    const showBlockingPanel = Services.prefs.getBoolPref(
      "devtools.netmonitor.features.requestBlocking"
    );
    const showSearchPanel = Services.prefs.getBoolPref(
      "devtools.netmonitor.features.search"
    );

    return div(
      { className: "network-action-bar" },
      Tabbar(
        {},
        showSearchPanel &&
          TabPanel(
            {
              id: "network-action-bar-search",
              title: L10N.getStr("netmonitor.actionbar.search"),
              className: "network-action-bar-search",
            },
            SearchPanel({ connector })
          ),
        showBlockingPanel &&
          TabPanel(
            {
              id: "network-action-bar-blocked",
              title: L10N.getStr("netmonitor.actionbar.requestBlocking"),
              className: "network-action-bar-blocked",
            },
            RequestBlockingPanel()
          )
      )
    );
  }
}

module.exports = NetworkActionBar;
