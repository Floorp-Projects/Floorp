/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const NEW_RDM_ENABLED = "devtools.responsive.html.enabled";

// If the new HTML RDM UI is enabled and e10s is enabled by default (e10s is required for
// the new HTML RDM UI to function), delegate the ResponsiveUIManager API over to that
// tool instead.  Performing this delegation here allows us to contain the pref check to a
// single place.
if (Services.prefs.getBoolPref(NEW_RDM_ENABLED) &&
    Services.appinfo.browserTabsRemoteAutostart) {
  let { ResponsiveUIManager } = require("devtools/client/responsive.html/manager");
  module.exports = ResponsiveUIManager;
} else {
  let { ResponsiveUIManager } = require("devtools/client/responsivedesign/responsivedesign-old");
  module.exports = ResponsiveUIManager;
}
