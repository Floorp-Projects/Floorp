"use strict";

const {PrefsHelper} = require("devtools/client/shared/prefs");

/**
 * Shortcuts for accessing various network monitor preferences.
 */

exports.Prefs = new PrefsHelper("devtools.netmonitor", {
  networkDetailsWidth: ["Int", "panes-network-details-width"],
  networkDetailsHeight: ["Int", "panes-network-details-height"],
  statistics: ["Bool", "statistics"],
  filters: ["Json", "filters"]
});
