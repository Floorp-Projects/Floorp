/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

export class AboutMessagePreviewChild extends JSWindowActorChild {
  handleEvent(event) {
    console.log(`Received page event ${event.type}`);
  }

  actorCreated() {
    this.exportFunctions();
  }

  exportFunctions() {
    if (this.contentWindow) {
      for (const name of ["MPShowMessage", "MPIsEnabled", "MPShouldShowHint"]) {
        Cu.exportFunction(this[name].bind(this), this.contentWindow, {
          defineAs: name,
        });
      }
    }
  }

  /**
   * Check if the Message Preview feature is enabled. This reflects the value of
   * the pref `browser.newtabpage.activity-stream.asrouter.devtoolsEnabled`.
   *
   * @returns {boolean}
   */
  MPIsEnabled() {
    return Services.prefs.getBoolPref(
      "browser.newtabpage.activity-stream.asrouter.devtoolsEnabled",
      false
    );
  }

  /**
   * Route a message to the parent process to be displayed with the relevant
   * messaging surface.
   *
   * @param {object} message
   */
  MPShowMessage(message) {
    this.sendAsyncMessage(`MessagePreview:SHOW_MESSAGE`, message);
  }

  /**
   * Check if a hint should be shown about how to enable Message Preview. The
   * hint is only displayed in local/unofficial builds.
   *
   * @returns {boolean}
   */
  MPShouldShowHint() {
    return !this.MPIsEnabled() && !AppConstants.MOZILLA_OFFICIAL;
  }
}
