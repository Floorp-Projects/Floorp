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

const PROCESS_COUNT_PREF = "dom.ipc.processCount";

module.exports = createClass({
  displayName: "multiE10SWarning",

  onUpdatePreferenceClick() {
    // Hardcoded string for Aurora 54
    // see (https://bugzilla.mozilla.org/show_bug.cgi?id=1345932#c44)
    let message = "Set “dom.ipc.processCount” to 1 and restart the browser?";
    if (window.confirm(message)) {
      Services.prefs.setIntPref(PROCESS_COUNT_PREF, 1);
      // Restart the browser.
      Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
    }
  },

  render() {
    // Hardcoded strings for Aurora 54
    // see (https://bugzilla.mozilla.org/show_bug.cgi?id=1345932#c44)
    let multiProcessWarningTitle = "Service Worker debugging is not compatible with " +
                                   "multiple content processes at the moment.";
    let multiProcessWarningMessage = "The preference “dom.ipc.processCount” can be set " +
                                     "to 1 to force a single content process.";
    let multiProcessWarningUpdateLink = "Set dom.ipc.processCount to 1";

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
