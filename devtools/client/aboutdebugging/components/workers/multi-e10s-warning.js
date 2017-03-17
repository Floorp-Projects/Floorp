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

const Strings = Services.strings.createBundle("chrome://devtools/locale/aboutdebugging.properties");
const PROCESS_COUNT_PREF = "dom.ipc.processCount";

module.exports = createClass({
  displayName: "multiE10SWarning",

  onUpdatePreferenceClick() {
    let message = Strings.GetStringFromName("multiProcessWarningConfirmUpdate");
    if (window.confirm(message)) {
      Services.prefs.setIntPref(PROCESS_COUNT_PREF, 1);
      // Restart the browser.
      Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
    }
  },

  render() {
    return dom.div(
      {
        className: "service-worker-multi-process"
      },
      dom.div(
        {},
        dom.div({ className: "warning" }),
        dom.b({}, Strings.GetStringFromName("multiProcessWarningTitle"))
      ),
      dom.div(
        {},
        Strings.GetStringFromName("multiProcessWarningMessage")
      ),
      dom.button(
        {
          className: "update-button",
          onClick: this.onUpdatePreferenceClick,
        },
        Strings.GetStringFromName("multiProcessWarningUpdateLink")
      )
    );
  },
});
