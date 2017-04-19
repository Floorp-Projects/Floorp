/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

loader.lazyImporter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
const { createClass, DOM: dom } =
  require("devtools/client/shared/vendor/react");
const Services = require("Services");
const { Ci } = require("chrome");

loader.lazyImporter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");

loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/main", true);

const MULTI_OPT_OUT_PREF = "dom.ipc.multiOptOut";

module.exports = createClass({
  displayName: "multiE10SWarning",

  onUpdatePreferenceClick() {
    // Hardcoded string for Beta 54
    // see (https://bugzilla.mozilla.org/show_bug.cgi?id=1345932#c44)
    let message = "Opt out of multiple processes?";
    if (window.confirm(message)) {
      // Disable multi until at least the next experiment.
      Services.prefs.setIntPref(MULTI_OPT_OUT_PREF,
                                Services.appinfo.E10S_MULTI_EXPERIMENT);
      // Restart the browser.
      Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
    }
  },

  render() {
    // Hardcoded strings for Beta 54
    // see (https://bugzilla.mozilla.org/show_bug.cgi?id=1345932#c44)
    let multiProcessWarningTitle = "Service Worker debugging is not compatible with " +
                                   "multiple content processes at the moment.";
    let multiProcessWarningMessage = `The preference “dom.ipc.multiOptOut” can be ` +
                                     `modified to force a single content process ` +
                                     `for the current version.`;
    let multiProcessWarningUpdateLink = "Opt out of multiple content processes";

    return dom.div(
      {
        className: "service-worker-multi-process"
      },
      dom.div(
        {},
        dom.div({ className: "warning" }),
        dom.b({}, multiProcessWarningTitle)
      ),
      dom.div(
        {},
        multiProcessWarningMessage
      ),
      dom.button(
        {
          className: "update-button",
          onClick: this.onUpdatePreferenceClick,
        },
        multiProcessWarningUpdateLink
      )
    );
  },
});
