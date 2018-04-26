/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const Services = require("Services");
const { Ci } = require("chrome");

const Strings = Services.strings.createBundle("chrome://devtools/locale/aboutdebugging.properties");
const MULTI_OPT_OUT_PREF = "dom.ipc.multiOptOut";

class multiE10SWarning extends Component {
  constructor(props) {
    super(props);
    this.onUpdatePreferenceClick = this.onUpdatePreferenceClick.bind(this);
  }

  onUpdatePreferenceClick() {
    let message = Strings.GetStringFromName("multiProcessWarningConfirmUpdate2");
    if (window.confirm(message)) {
      // Disable multi until at least the next experiment.
      Services.prefs.setIntPref(MULTI_OPT_OUT_PREF,
                                Services.appinfo.E10S_MULTI_EXPERIMENT);
      // Restart the browser.
      Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
    }
  }

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
        Strings.GetStringFromName("multiProcessWarningMessage2")
      ),
      dom.button(
        {
          className: "update-button",
          onClick: this.onUpdatePreferenceClick,
        },
        Strings.GetStringFromName("multiProcessWarningUpdateLink2")
      )
    );
  }
}

module.exports = multiE10SWarning;
