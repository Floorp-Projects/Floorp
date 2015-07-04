# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

let TrackingProtection = {
  PREF_ENABLED: "privacy.trackingprotection.enabled",

  init() {
    let $ = selector => document.querySelector(selector);
    this.container = $("#tracking-protection-container");
    this.content = $("#tracking-protection-content");

    this.updateEnabled();
    Services.prefs.addObserver(this.PREF_ENABLED, this, false);

    this.enabledHistogram.add(this.enabled);
  },

  uninit() {
    Services.prefs.removeObserver(this.PREF_ENABLED, this);
  },

  observe() {
    this.updateEnabled();
  },

  updateEnabled() {
    this.enabled = Services.prefs.getBoolPref(this.PREF_ENABLED);
    this.container.hidden = !this.enabled;
  },

  get enabledHistogram() {
    return Services.telemetry.getHistogramById("TRACKING_PROTECTION_ENABLED");
  },

  get eventsHistogram() {
    return Services.telemetry.getHistogramById("TRACKING_PROTECTION_EVENTS");
  },

  onSecurityChange(state) {
    if (!this.enabled) {
      return;
    }

    let {
      STATE_BLOCKED_TRACKING_CONTENT, STATE_LOADED_TRACKING_CONTENT
    } = Ci.nsIWebProgressListener;

    if (state & STATE_BLOCKED_TRACKING_CONTENT) {
      this.content.setAttribute("block-active", true);
      this.content.removeAttribute("block-disabled");
    } else if (state & STATE_LOADED_TRACKING_CONTENT) {
      this.content.setAttribute("block-disabled", true);
      this.content.removeAttribute("block-active");
    } else {
      this.content.removeAttribute("block-disabled");
      this.content.removeAttribute("block-active");
    }

    // Telemetry for state change.
    this.eventsHistogram.add(0);
  },

  disableForCurrentPage() {
    // Convert document URI into the format used by
    // nsChannelClassifier::ShouldEnableTrackingProtection.
    // Any scheme turned into https is correct.
    let normalizedUrl = Services.io.newURI(
      "https://" + gBrowser.selectedBrowser.currentURI.hostPort,
      null, null);

    // Add the current host in the 'trackingprotection' consumer of
    // the permission manager using a normalized URI. This effectively
    // places this host on the tracking protection allowlist.
    Services.perms.add(normalizedUrl,
      "trackingprotection", Services.perms.ALLOW_ACTION);

    // Telemetry for disable protection.
    this.eventsHistogram.add(1);

    BrowserReload();
  },

  enableForCurrentPage() {
    // Remove the current host from the 'trackingprotection' consumer
    // of the permission manager. This effectively removes this host
    // from the tracking protection allowlist.
    Services.perms.remove(gBrowser.selectedBrowser.currentURI,
      "trackingprotection");

    // Telemetry for enable protection.
    this.eventsHistogram.add(2);

    BrowserReload();
  },
};
