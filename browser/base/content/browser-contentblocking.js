/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var TrackingProtection = {
  reportBreakageLabel: "trackingprotection",
  telemetryIdentifier: "tp",
  PREF_ENABLED_GLOBALLY: "privacy.trackingprotection.enabled",
  PREF_ENABLED_IN_PRIVATE_WINDOWS: "privacy.trackingprotection.pbmode.enabled",
  PREF_UI_ENABLED: "browser.contentblocking.trackingprotection.control-center.ui.enabled",
  enabledGlobally: false,
  enabledInPrivateWindows: false,

  get categoryItem() {
    delete this.categoryItem;
    return this.categoryItem =
      document.getElementById("identity-popup-content-blocking-category-tracking-protection");
  },

  init() {
    this.updateEnabled();

    Services.prefs.addObserver(this.PREF_ENABLED_GLOBALLY, this);
    Services.prefs.addObserver(this.PREF_ENABLED_IN_PRIVATE_WINDOWS, this);

    XPCOMUtils.defineLazyPreferenceGetter(this, "visible", this.PREF_UI_ENABLED, false);
  },

  uninit() {
    Services.prefs.removeObserver(this.PREF_ENABLED_GLOBALLY, this);
    Services.prefs.removeObserver(this.PREF_ENABLED_IN_PRIVATE_WINDOWS, this);
  },

  observe() {
    this.updateEnabled();
  },

  get enabled() {
    return this.enabledGlobally ||
           (this.enabledInPrivateWindows &&
            PrivateBrowsingUtils.isWindowPrivate(window));
  },

  updateEnabled() {
    this.enabledGlobally =
      Services.prefs.getBoolPref(this.PREF_ENABLED_GLOBALLY);
    this.enabledInPrivateWindows =
      Services.prefs.getBoolPref(this.PREF_ENABLED_IN_PRIVATE_WINDOWS);
  },

  isBlockerActivated(state) {
    return state & Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT;
  },
};

var ThirdPartyCookies = {
  telemetryIdentifier: "cr",
  PREF_ENABLED: "network.cookie.cookieBehavior",
  PREF_REPORT_BREAKAGE_ENABLED: "browser.contentblocking.rejecttrackers.reportBreakage.enabled",
  PREF_ENABLED_VALUES: [
    // These values match the ones exposed under the Content Blocking section
    // of the Preferences UI.
    Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,  // Block all third-party cookies
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,  // Block third-party cookies from trackers
  ],
  PREF_UI_ENABLED: "browser.contentblocking.rejecttrackers.control-center.ui.enabled",

  get categoryItem() {
    delete this.categoryItem;
    return this.categoryItem =
      document.getElementById("identity-popup-content-blocking-category-3rdpartycookies");
  },

  get reportBreakageLabel() {
    switch (this.behaviorPref) {
    case Ci.nsICookieService.BEHAVIOR_ACCEPT:
      return "nocookiesblocked";
    case Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN:
      return "allthirdpartycookiesblocked";
    case Ci.nsICookieService.BEHAVIOR_REJECT:
      return "allcookiesblocked";
    case Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN:
      return "cookiesfromunvisitedsitesblocked";
    default:
      Cu.reportError(`Error: Unknown cookieBehavior pref observed: ${this.behaviorPref}`);
      // fall through
    case Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER:
      return "cookierestrictions";
    }
  },

  get categoryLabelDefault() {
    delete this.categoryLabelDefault;
    return this.categoryLabelDefault =
      document.getElementById("identity-popup-content-blocking-category-label-default");
  },

  get categoryLabelTrackers() {
    delete this.categoryLabelTrackers;
    return this.categoryLabelTrackers =
      document.getElementById("identity-popup-content-blocking-category-label-trackers");
  },

  updateCategoryLabel() {
    let rejectTrackers = this.behaviorPref == Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;
    this.categoryLabelDefault.hidden = rejectTrackers;
    this.categoryLabelTrackers.hidden = !rejectTrackers;
  },

  init() {
    XPCOMUtils.defineLazyPreferenceGetter(this, "behaviorPref", this.PREF_ENABLED,
      Ci.nsICookieService.BEHAVIOR_ACCEPT, this.updateCategoryLabel.bind(this));
    XPCOMUtils.defineLazyPreferenceGetter(this, "visible", this.PREF_UI_ENABLED, false);
    XPCOMUtils.defineLazyPreferenceGetter(this, "reportBreakageEnabled",
      this.PREF_REPORT_BREAKAGE_ENABLED, false);
    this.updateCategoryLabel();
  },
  get enabled() {
    return this.PREF_ENABLED_VALUES.includes(this.behaviorPref);
  },

  isBlockerActivated(state) {
    return (state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER) != 0 ||
           (state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_FOREIGN) != 0;
  },
};


var ContentBlocking = {
  // If the user ignores the doorhanger, we stop showing it after some time.
  MAX_INTROS: 20,
  PREF_ANIMATIONS_ENABLED: "toolkit.cosmeticAnimations.enabled",
  PREF_REPORT_BREAKAGE_ENABLED: "browser.contentblocking.reportBreakage.enabled",
  PREF_REPORT_BREAKAGE_URL: "browser.contentblocking.reportBreakage.url",
  PREF_INTRO_COUNT_CB: "browser.contentblocking.introCount",
  content: null,
  icon: null,
  activeTooltipText: null,
  disabledTooltipText: null,

  get prefIntroCount() {
    return this.PREF_INTRO_COUNT_CB;
  },

  get appMenuLabel() {
    delete this.appMenuLabel;
    return this.appMenuLabel = document.getElementById("appMenu-tp-label");
  },

  strings: {
    get appMenuTitle() {
      delete this.appMenuTitle;
      return this.appMenuTitle =
        gNavigatorBundle.getString("contentBlocking.title");
    },

    get appMenuTooltip() {
      delete this.appMenuTooltip;
      return this.appMenuTooltip =
        gNavigatorBundle.getString("contentBlocking.tooltip");
    },
  },

  // A list of blockers that will be displayed in the categories list
  // when blockable content is detected. A blocker must be an object
  // with at least the following two properties:
  //  - enabled: Whether the blocker is currently turned on.
  //  - categoryItem: The DOM item that represents the entry in the category list.
  //
  // It may also contain an init() and uninit() function, which will be called
  // on ContentBlocking.init() and ContentBlocking.uninit().
  blockers: [TrackingProtection, ThirdPartyCookies],

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
    this.content = $("#identity-popup-content-blocking-content");
    this.icon = $("#tracking-protection-icon");
    this.iconBox = $("#tracking-protection-icon-box");
    this.animatedIcon = $("#tracking-protection-icon-animatable-image");
    this.animatedIcon.addEventListener("animationend", () => this.iconBox.removeAttribute("animate"));

    this.identityPopupMultiView = $("#identity-popup-multiView");
    this.reportBreakageButton = $("#identity-popup-content-blocking-report-breakage");
    this.reportBreakageURL = $("#identity-popup-breakageReportView-collection-url");
    this.reportBreakageLearnMore = $("#identity-popup-breakageReportView-learn-more");

    let baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");
    this.reportBreakageLearnMore.href = baseURL + "blocking-breakage";

    this.updateAnimationsEnabled = () => {
      this.iconBox.toggleAttribute("animationsenabled",
        Services.prefs.getBoolPref(this.PREF_ANIMATIONS_ENABLED, false));
    };

    for (let blocker of this.blockers) {
      if (blocker.init) {
        blocker.init();
      }
    }

    this.updateAnimationsEnabled();

    Services.prefs.addObserver(this.PREF_ANIMATIONS_ENABLED, this.updateAnimationsEnabled);

    XPCOMUtils.defineLazyPreferenceGetter(this, "reportBreakageEnabled",
      this.PREF_REPORT_BREAKAGE_ENABLED, false);

    this.appMenuLabel.setAttribute("label", this.strings.appMenuTitle);
    this.appMenuLabel.setAttribute("tooltiptext", this.strings.appMenuTooltip);

    this.activeTooltipText =
      gNavigatorBundle.getString("trackingProtection.icon.activeTooltip");
    this.disabledTooltipText =
      gNavigatorBundle.getString("trackingProtection.icon.disabledTooltip");
  },

  uninit() {
    for (let blocker of this.blockers) {
      if (blocker.uninit) {
        blocker.uninit();
      }
    }

    Services.prefs.removeObserver(this.PREF_ANIMATIONS_ENABLED, this.updateAnimationsEnabled);
  },

  hideIdentityPopupAndReload() {
    document.getElementById("identity-popup").hidePopup();
    BrowserReload();
  },

  openPreferences(origin) {
    openPreferences("privacy-trackingprotection", { origin });
  },

  backToMainView() {
    this.identityPopupMultiView.goBack();
  },

  submitBreakageReport() {
    document.getElementById("identity-popup").hidePopup();

    let reportEndpoint = Services.prefs.getStringPref(this.PREF_REPORT_BREAKAGE_URL);
    if (!reportEndpoint) {
      return;
    }

    let formData = new FormData();
    formData.set("title", this.reportURI.host);

    // Leave the ? at the end of the URL to signify that this URL had its query stripped.
    let urlWithoutQuery = this.reportURI.asciiSpec.replace(this.reportURI.query, "");
    let body = `Full URL: ${urlWithoutQuery}\n`;
    body += `userAgent: ${navigator.userAgent}\n`;

    body += "\n**Preferences**\n";
    body += `${TrackingProtection.PREF_ENABLED_GLOBALLY}: ${Services.prefs.getBoolPref(TrackingProtection.PREF_ENABLED_GLOBALLY)}\n`;
    body += `${TrackingProtection.PREF_ENABLED_IN_PRIVATE_WINDOWS}: ${Services.prefs.getBoolPref(TrackingProtection.PREF_ENABLED_IN_PRIVATE_WINDOWS)}\n`;
    body += `${TrackingProtection.PREF_UI_ENABLED}: ${Services.prefs.getBoolPref(TrackingProtection.PREF_UI_ENABLED)}\n`;
    body += `urlclassifier.trackingTable: ${Services.prefs.getStringPref("urlclassifier.trackingTable")}\n`;
    body += `network.http.referer.defaultPolicy: ${Services.prefs.getIntPref("network.http.referer.defaultPolicy")}\n`;
    body += `network.http.referer.defaultPolicy.pbmode: ${Services.prefs.getIntPref("network.http.referer.defaultPolicy.pbmode")}\n`;
    body += `${ThirdPartyCookies.PREF_UI_ENABLED}: ${Services.prefs.getBoolPref(ThirdPartyCookies.PREF_UI_ENABLED)}\n`;
    body += `${ThirdPartyCookies.PREF_ENABLED}: ${Services.prefs.getIntPref(ThirdPartyCookies.PREF_ENABLED)}\n`;
    body += `network.cookie.lifetimePolicy: ${Services.prefs.getIntPref("network.cookie.lifetimePolicy")}\n`;
    body += `privacy.restrict3rdpartystorage.expiration: ${Services.prefs.getIntPref("privacy.restrict3rdpartystorage.expiration")}\n`;

    let comments = document.getElementById("identity-popup-breakageReportView-collection-comments");
    body += "\n**Comments**\n" + comments.value;

    formData.set("body", body);

    let activatedBlockers = [];
    for (let blocker of this.blockers) {
      if (blocker.activated) {
        activatedBlockers.push(blocker.reportBreakageLabel);
      }
    }

    if (activatedBlockers.length) {
      formData.set("labels", activatedBlockers.join(","));
    }

    fetch(reportEndpoint, {
      method: "POST",
      credentials: "omit",
      body: formData,
    }).then(function(response) {
      if (!response.ok) {
        Cu.reportError(`Content Blocking report to ${reportEndpoint} failed with status ${response.status}`);
      }
    }).catch(Cu.reportError);
  },

  showReportBreakageSubview() {
    // Save this URI to make sure that the user really only submits the location
    // they see in the report breakage dialog.
    this.reportURI = gBrowser.currentURI;
    let urlWithoutQuery = this.reportURI.asciiSpec.replace("?" + this.reportURI.query, "");
    this.reportBreakageURL.textContent = urlWithoutQuery;
    this.identityPopupMultiView.showSubView("identity-popup-breakageReportView");
  },

  shieldHistogramAdd(value) {
    if (PrivateBrowsingUtils.isWindowPrivate(window)) {
      return;
    }
    Services.telemetry.getHistogramById("TRACKING_PROTECTION_SHIELD").add(value);
  },

  onSecurityChange(oldState, state, webProgress, isSimulated,
                   contentBlockingLogJSON) {
    let baseURI = this._baseURIForChannelClassifier;

    // Don't deal with about:, file: etc.
    if (!baseURI) {
      this.iconBox.removeAttribute("animate");
      this.iconBox.removeAttribute("active");
      this.iconBox.removeAttribute("hasException");
      return;
    }

    // The user might have navigated before the shield animation
    // finished. In this case, reset the animation to be able to
    // play it in full again and avoid choppiness.
    if (webProgress.isTopLevel) {
      this.iconBox.removeAttribute("animate");
    }

    let anyBlockerActivated = false;

    for (let blocker of this.blockers) {
      // Store data on whether the blocker is activated in the current document for
      // reporting it using the "report breakage" dialog. Under normal circumstances this
      // dialog should only be able to open in the currently selected tab and onSecurityChange
      // runs on tab switch, so we can avoid associating the data with the document directly.
      blocker.activated = blocker.isBlockerActivated(state);
      blocker.categoryItem.classList.toggle("blocked", blocker.enabled);
      blocker.categoryItem.hidden = !blocker.visible;
      anyBlockerActivated = anyBlockerActivated || blocker.activated;
    }

    // We consider the shield state "active" when some kind of blocking activity
    // occurs on the page.  Note that merely allowing the loading of content that
    // we could have blocked does not trigger the appearance of the shield.
    // This state will be overriden later if there's an exception set for this site.
    let active = anyBlockerActivated;
    let isAllowing = state & Ci.nsIWebProgressListener.STATE_LOADED_TRACKING_CONTENT;
    let detected = anyBlockerActivated || isAllowing;

    let isBrowserPrivate = PrivateBrowsingUtils.isBrowserPrivate(gBrowser.selectedBrowser);

    // Check whether the user has added an exception for this site.
    let type =  isBrowserPrivate ? "trackingprotection-pb" : "trackingprotection";
    let hasException = Services.perms.testExactPermission(baseURI, type) ==
      Services.perms.ALLOW_ACTION;

    this.content.toggleAttribute("detected", detected);
    this.content.toggleAttribute("hasException", hasException);
    this.content.toggleAttribute("active", active);

    this.iconBox.toggleAttribute("active", active);
    this.iconBox.toggleAttribute("hasException", hasException);

    // For release (due to the large volume) we only want to receive reports
    // for breakage that is directly related to third party cookie blocking.
    if (this.reportBreakageEnabled ||
        (ThirdPartyCookies.reportBreakageEnabled &&
         ThirdPartyCookies.activated &&
         !TrackingProtection.activated)) {
      this.reportBreakageButton.removeAttribute("hidden");
    } else {
      this.reportBreakageButton.setAttribute("hidden", "true");
    }

    if (isSimulated) {
      this.iconBox.removeAttribute("animate");
    } else if (active && webProgress.isTopLevel) {
      this.iconBox.setAttribute("animate", "true");

      if (!isBrowserPrivate) {
        let introCount = Services.prefs.getIntPref(this.prefIntroCount);
        if (introCount < this.MAX_INTROS) {
          Services.prefs.setIntPref(this.prefIntroCount, ++introCount);
          Services.prefs.savePrefFile(null);
          this.showIntroPanel();
        }
      }
    }

    if (hasException) {
      this.iconBox.setAttribute("tooltiptext", this.disabledTooltipText);
      this.shieldHistogramAdd(1);
    } else if (active) {
      this.iconBox.setAttribute("tooltiptext", this.activeTooltipText);
      this.shieldHistogramAdd(2);
    } else {
      this.iconBox.removeAttribute("tooltiptext");
      this.shieldHistogramAdd(0);
    }
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

    this.hideIdentityPopupAndReload();
  },

  dontShowIntroPanelAgain() {
    if (!PrivateBrowsingUtils.isBrowserPrivate(gBrowser.selectedBrowser)) {
      Services.prefs.setIntPref(this.prefIntroCount, this.MAX_INTROS);
      Services.prefs.savePrefFile(null);
    }
  },

  async showIntroPanel() {
    let brandBundle = document.getElementById("bundle_brand");
    let brandShortName = brandBundle.getString("brandShortName");


    let introTitle = gNavigatorBundle.getFormattedString("contentBlocking.intro.title",
                                                         [brandShortName]);
    let introDescription;
    // This will be sent to the onboarding website to let them know which
    // UI variation we're showing.
    let variation;
    // We show a different UI tour variation for users that already have TP
    // enabled globally.
    if (TrackingProtection.enabledGlobally) {
      introDescription = gNavigatorBundle.getString("contentBlocking.intro.v2.description");
      variation = 2;
    } else {
      introDescription = gNavigatorBundle.getFormattedString("contentBlocking.intro.v1.description",
                                                             [brandShortName]);
      variation = 1;
    }

    let openStep2 = () => {
      // When the user proceeds in the tour, adjust the counter to indicate that
      // the user doesn't need to see the intro anymore.
      this.dontShowIntroPanelAgain();

      let nextURL = Services.urlFormatter.formatURLPref("privacy.trackingprotection.introURL") +
                    `?step=2&newtab=true&variation=${variation}`;
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
    UITour.showInfo(window, panelTarget, introTitle, introDescription, undefined, buttons,
                    { closeButtonCallback: () => this.dontShowIntroPanelAgain() });
  },
};
