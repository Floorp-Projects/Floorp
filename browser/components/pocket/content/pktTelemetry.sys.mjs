/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.sys.mjs",
  pktApi: "chrome://pocket/content/pktApi.sys.mjs",
});

const PREF_IMPRESSION_ID = "browser.newtabpage.activity-stream.impressionId";

export var pktTelemetry = {
  get impressionId() {
    if (!this._impressionId) {
      this._impressionId = this.getOrCreateImpressionId();
    }
    return this._impressionId;
  },

  // Sets or gets the impression id that's use for Pocket impressions.
  // The impression id cannot be tied to a client id.
  // This is the same impression id used in newtab pocket impressions.
  getOrCreateImpressionId() {
    let impressionId = Services.prefs.getStringPref(PREF_IMPRESSION_ID, "");

    if (!impressionId) {
      impressionId = String(Services.uuid.generateUUID());
      Services.prefs.setStringPref(PREF_IMPRESSION_ID, impressionId);
    }
    return impressionId;
  },

  _profileCreationDate() {
    return (
      lazy.TelemetryEnvironment.currentEnvironment.profile.resetDate ||
      lazy.TelemetryEnvironment.currentEnvironment.profile.creationDate
    );
  },

  /**
   * Records the provided data and common pocket-button data to Glean,
   * then submits it all in a pocket-button ping.
   *
   * @param eventAction - A string like "click"
   * @param eventSource - A string like "save_button"
   * @param eventPosition - (optional) A 0-based index.
   *                        If falsey and not 0, is coalesced to undefined.
   * @param model - (optional) An identifier for the machine learning model
   *                used to generate the recommendations like "vec-bestarticle"
   */
  submitPocketButtonPing(
    eventAction,
    eventSource,
    eventPosition = undefined,
    model = undefined
  ) {
    eventPosition = eventPosition || eventPosition === 0 ? 0 : undefined;
    Glean.pocketButton.impressionId.set(this.impressionId);
    Glean.pocketButton.pocketLoggedInStatus.set(lazy.pktApi.isUserLoggedIn());
    Glean.pocketButton.profileCreationDate.set(this._profileCreationDate());

    Glean.pocketButton.eventAction.set(eventAction);
    Glean.pocketButton.eventSource.set(eventSource);
    if (eventPosition !== undefined) {
      Glean.pocketButton.eventPosition.set(eventPosition);
    }
    if (model !== undefined) {
      Glean.pocketButton.model.set(model);
    }

    GleanPings.pocketButton.submit();
  },
};
