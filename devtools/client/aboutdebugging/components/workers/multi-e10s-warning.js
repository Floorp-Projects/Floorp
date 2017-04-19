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
const MULTI_OPT_OUT_PREF = "dom.ipc.multiOptOut";

module.exports = createClass({
  displayName: "multiE10SWarning",

  onUpdatePreferenceClick() {
    let message = Strings.GetStringFromName("multiProcessWarningConfirmUpdate");
    if (window.confirm(message)) {
      // Disable multi until at least the next experiment.
      Services.prefs.setIntPref(MULTI_OPT_OUT_PREF,
                                Services.appinfo.E10S_MULTI_EXPERIMENT);
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
