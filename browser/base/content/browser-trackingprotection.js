/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var TrackingProtection = {
  // If the user ignores the doorhanger, we stop showing it after some time.
  MAX_INTROS: 20,
  PREF_ENABLED_GLOBALLY: "privacy.trackingprotection.enabled",
  PREF_ENABLED_IN_PRIVATE_WINDOWS: "privacy.trackingprotection.pbmode.enabled",
  PREF_ANIMATIONS_ENABLED: "toolkit.cosmeticAnimations.enabled",
  enabledGlobally: false,
  enabledInPrivateWindows: false,
  container: null,
  content: null,
  icon: null,
  activeTooltipText: null,
  disabledTooltipText: null,

  get _baseURIForChannelClassifier() {
    // Convert document URI into the format used by
    // nsChannelClassifier::ShouldEnableTrackingProtection.
    // Any scheme turned into https is correct.
    try {
      return Services.io.newURI("https://" + gBrowser.selectedBrowser.currentURI.hostPort);
    } catch (e) {
      // Getting the hostPort for about: and file: URIs fails, but TP doesn't work with
      // these URIs anyway, so just return null here.
      return null;
    }
  },

  init() {
    let $ = selector => document.querySelector(selector);
    this.container = $("#tracking-protection-container");
    this.content = $("#tracking-protection-content");
    this.icon = $("#tracking-protection-icon");
    this.iconBox = $("#tracking-protection-icon-box");
    this.animatedIcon = $("#tracking-protection-icon-animatable-image");
    this.animatedIcon.addEventListener("animationend", () => this.iconBox.removeAttribute("animate"));

    this.broadcaster = $("#trackingProtectionBroadcaster");

    this.enableTooltip =
      gNavigatorBundle.getString("trackingProtection.toggle.enable.tooltip");
    this.disableTooltip =
      gNavigatorBundle.getString("trackingProtection.toggle.disable.tooltip");
    this.enableTooltipPB =
      gNavigatorBundle.getString("trackingProtection.toggle.enable.pbmode.tooltip");
    this.disableTooltipPB =
      gNavigatorBundle.getString("trackingProtection.toggle.disable.pbmode.tooltip");

    this.updateAnimationsEnabled = () => {
      this.iconBox.toggleAttribute("animationsenabled",
        Services.prefs.getBoolPref(this.PREF_ANIMATIONS_ENABLED, false));
    };

    this.updateAnimationsEnabled();

    Services.prefs.addObserver(this.PREF_ANIMATIONS_ENABLED, this.updateAnimationsEnabled);

    this.updateEnabled();
    Services.prefs.addObserver(this.PREF_ENABLED_GLOBALLY, this);
    Services.prefs.addObserver(this.PREF_ENABLED_IN_PRIVATE_WINDOWS, this);

    this.activeTooltipText =
      gNavigatorBundle.getString("trackingProtection.icon.activeTooltip");
    this.disabledTooltipText =
      gNavigatorBundle.getString("trackingProtection.icon.disabledTooltip");

    this.enabledHistogramAdd(this.enabledGlobally);
    this.disabledPBMHistogramAdd(!this.enabledInPrivateWindows);
  },

  uninit() {
    Services.prefs.removeObserver(this.PREF_ENABLED_GLOBALLY, this);
    Services.prefs.removeObserver(this.PREF_ENABLED_IN_PRIVATE_WINDOWS, this);
    Services.prefs.removeObserver(this.PREF_ANIMATIONS_ENABLED, this.updateAnimationsEnabled);
  },

  observe() {
    this.updateEnabled();
  },

  get enabled() {
    return this.enabledGlobally ||
           (this.enabledInPrivateWindows &&
            PrivateBrowsingUtils.isWindowPrivate(window));
  },

  onGlobalToggleCommand() {
    if (PrivateBrowsingUtils.isWindowPrivate(window)) {
      Services.prefs.setBoolPref(this.PREF_ENABLED_IN_PRIVATE_WINDOWS, !this.enabledInPrivateWindows);
    } else {
      Services.prefs.setBoolPref(this.PREF_ENABLED_GLOBALLY, !this.enabledGlobally);
    }
  },

  hideIdentityPopupAndReload() {
    document.getElementById("identity-popup").hidePopup();
    BrowserReload();
  },

  openPreferences(origin) {
    openPreferences("privacy-trackingprotection", { origin });
  },

  updateEnabled() {
    this.enabledGlobally =
      Services.prefs.getBoolPref(this.PREF_ENABLED_GLOBALLY);
    this.enabledInPrivateWindows =
      Services.prefs.getBoolPref(this.PREF_ENABLED_IN_PRIVATE_WINDOWS);

    this.content.setAttribute("enabled", this.enabled);

    if (PrivateBrowsingUtils.isWindowPrivate(window)) {
      this.broadcaster.setAttribute("enabled", this.enabledInPrivateWindows);
      this.broadcaster.setAttribute("aria-pressed", this.enabledInPrivateWindows);
      this.broadcaster.setAttribute("tooltiptext", this.enabledInPrivateWindows ?
        this.disableTooltipPB : this.enableTooltipPB);
    } else {
      this.broadcaster.setAttribute("enabled", this.enabledGlobally);
      this.broadcaster.setAttribute("aria-pressed", this.enabledGlobally);
      this.broadcaster.setAttribute("tooltiptext", this.enabledGlobally ?
        this.disableTooltip : this.enableTooltip);
    }
  },

  enabledHistogramAdd(value) {
    if (PrivateBrowsingUtils.isWindowPrivate(window)) {
      return;
    }
    Services.telemetry.getHistogramById("TRACKING_PROTECTION_ENABLED").add(value);
  },

  disabledPBMHistogramAdd(value) {
    if (PrivateBrowsingUtils.isWindowPrivate(window)) {
      return;
    }
    Services.telemetry.getHistogramById("TRACKING_PROTECTION_PBM_DISABLED").add(value);
  },

  eventsHistogramAdd(value) {
    if (PrivateBrowsingUtils.isWindowPrivate(window)) {
      return;
    }
    Services.telemetry.getHistogramById("TRACKING_PROTECTION_EVENTS").add(value);
  },

  shieldHistogramAdd(value) {
    if (PrivateBrowsingUtils.isWindowPrivate(window)) {
      return;
    }
    Services.telemetry.getHistogramById("TRACKING_PROTECTION_SHIELD").add(value);
  },

  cancelAnimation() {
    let iconAnimation = this.animatedIcon.getAnimations()[0];
    if (iconAnimation && iconAnimation.currentTime) {
      iconAnimation.cancel();
    }
    this.iconBox.removeAttribute("animate");
  },

  onSecurityChange(state, webProgress, isSimulated) {
    let baseURI = this._baseURIForChannelClassifier;

    // Don't deal with about:, file: etc.
    if (!baseURI) {
      this.cancelAnimation();
      this.iconBox.removeAttribute("state");
      return;
    }

    // The user might have navigated before the shield animation
    // finished. In this case, reset the animation to be able to
    // play it in full again and avoid choppiness.
    if (webProgress.isTopLevel) {
      this.cancelAnimation();
    }

    let isBlocking = state & Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT;
    let isAllowing = state & Ci.nsIWebProgressListener.STATE_LOADED_TRACKING_CONTENT;

    // Check whether the user has added an exception for this site.
    let hasException = false;
    if (PrivateBrowsingUtils.isBrowserPrivate(gBrowser.selectedBrowser)) {
      hasException = PrivateBrowsingUtils.existsInTrackingAllowlist(baseURI);
    } else {
      hasException = Services.perms.testExactPermission(baseURI,
        "trackingprotection") == Services.perms.ALLOW_ACTION;
    }

    if (hasException) {
      this.content.setAttribute("hasException", "true");
    } else {
      this.content.removeAttribute("hasException");
    }

    if (isBlocking && this.enabled) {
      if (isSimulated) {
        this.cancelAnimation();
      } else if (webProgress.isTopLevel) {
        this.iconBox.setAttribute("animate", "true");
      }

      this.iconBox.setAttribute("tooltiptext", this.activeTooltipText);
      this.iconBox.setAttribute("state", "blocked-tracking-content");
      this.content.setAttribute("state", "blocked-tracking-content");

      // Open the tracking protection introduction panel, if applicable.
      if (this.enabledGlobally) {
        let introCount = Services.prefs.getIntPref("privacy.trackingprotection.introCount");
        if (introCount < TrackingProtection.MAX_INTROS) {
          Services.prefs.setIntPref("privacy.trackingprotection.introCount", ++introCount);
          Services.prefs.savePrefFile(null);
          this.showIntroPanel();
        }
      }

      this.shieldHistogramAdd(2);
    } else if (isAllowing) {
      if (isSimulated) {
        this.cancelAnimation();
      } else if (webProgress.isTopLevel) {
        this.iconBox.setAttribute("animate", "true");
      }

      // Only show the shield when TP is enabled for now.
      if (this.enabled) {
        this.iconBox.setAttribute("tooltiptext", this.disabledTooltipText);
        this.iconBox.setAttribute("state", "loaded-tracking-content");
        this.shieldHistogramAdd(1);
      } else {
        this.iconBox.removeAttribute("tooltiptext");
        this.iconBox.removeAttribute("state");
        this.shieldHistogramAdd(0);
      }

      // Warn in the control center even with TP disabled.
      this.content.setAttribute("state", "loaded-tracking-content");
    } else {
      this.iconBox.removeAttribute("tooltiptext");
      this.iconBox.removeAttribute("state");
      this.content.removeAttribute("state");

      // We didn't show the shield
      this.shieldHistogramAdd(0);
    }

    // Telemetry for state change.
    this.eventsHistogramAdd(0);
  },

  disableForCurrentPage() {
    let baseURI = this._baseURIForChannelClassifier;

    // Add the current host in the 'trackingprotection' consumer of
    // the permission manager using a normalized URI. This effectively
    // places this host on the tracking protection allowlist.
    if (PrivateBrowsingUtils.isBrowserPrivate(gBrowser.selectedBrowser)) {
      PrivateBrowsingUtils.addToTrackingAllowlist(baseURI);
    } else {
      Services.perms.add(baseURI,
        "trackingprotection", Services.perms.ALLOW_ACTION);
    }

    // Telemetry for disable protection.
    this.eventsHistogramAdd(1);

    this.hideIdentityPopupAndReload();
  },

  enableForCurrentPage() {
    // Remove the current host from the 'trackingprotection' consumer
    // of the permission manager. This effectively removes this host
    // from the tracking protection allowlist.
    let baseURI = this._baseURIForChannelClassifier;

    if (PrivateBrowsingUtils.isBrowserPrivate(gBrowser.selectedBrowser)) {
      PrivateBrowsingUtils.removeFromTrackingAllowlist(baseURI);
    } else {
      Services.perms.remove(baseURI, "trackingprotection");
    }

    // Telemetry for enable protection.
    this.eventsHistogramAdd(2);

    this.hideIdentityPopupAndReload();
  },

  dontShowIntroPanelAgain() {
    // This function may be called in private windows, but it does not change
    // any preference unless Tracking Protection is enabled globally.
    if (this.enabledGlobally) {
      Services.prefs.setIntPref("privacy.trackingprotection.introCount",
                                this.MAX_INTROS);
      Services.prefs.savePrefFile(null);
    }
  },

  async showIntroPanel() {
    let brandBundle = document.getElementById("bundle_brand");
    let brandShortName = brandBundle.getString("brandShortName");

    let openStep2 = () => {
      // When the user proceeds in the tour, adjust the counter to indicate that
      // the user doesn't need to see the intro anymore.
      this.dontShowIntroPanelAgain();

      let nextURL = Services.urlFormatter.formatURLPref("privacy.trackingprotection.introURL") +
                    "?step=2&newtab=true";
      switchToTabHavingURI(nextURL, true, {
        // Ignore the fragment in case the intro is shown on the tour page
        // (e.g. if the user manually visited the tour or clicked the link from
        // about:privatebrowsing) so we can avoid a reload.
        ignoreFragment: "whenComparingAndReplace",
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      });
    };

    let buttons = [
      {
        label: gNavigatorBundle.getString("trackingProtection.intro.step1of3"),
        style: "text",
      },
      {
        callback: openStep2,
        label: gNavigatorBundle.getString("trackingProtection.intro.nextButton.label"),
        style: "primary",
      },
    ];

    let panelTarget = await UITour.getTarget(window, "trackingProtection");
    UITour.initForBrowser(gBrowser.selectedBrowser, window);
    UITour.showInfo(window, panelTarget,
                    gNavigatorBundle.getString("trackingProtection.intro.title"),
                    gNavigatorBundle.getFormattedString("trackingProtection.intro.description2",
                                                        [brandShortName]),
                    undefined, buttons,
                    { closeButtonCallback: () => this.dontShowIntroPanelAgain() });
  },
};
