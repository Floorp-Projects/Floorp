/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Fingerprinting = {
  PREF_ENABLED: "privacy.trackingprotection.fingerprinting.enabled",
  telemetryIdentifier: "fp",

  strings: {
    get subViewBlocked() {
      delete this.subViewBlocked;
      return this.subViewBlocked =
        gNavigatorBundle.getString("contentBlocking.fingerprintersView.blocked.label");
    },
  },

  init() {
    XPCOMUtils.defineLazyPreferenceGetter(this, "enabled", this.PREF_ENABLED, false,
      () => this.updateCategoryLabel());
    this.updateCategoryLabel();
  },

  get categoryItem() {
    delete this.categoryItem;
    return this.categoryItem =
      document.getElementById("identity-popup-content-blocking-category-fingerprinters");
  },

  get categoryLabel() {
    delete this.categoryLabel;
    return this.categoryLabel =
      document.getElementById("identity-popup-content-blocking-fingerprinters-state-label");
  },

  get subViewList() {
    delete this.subViewList;
    return this.subViewList = document.getElementById("identity-popup-fingerprintersView-list");
  },

  updateCategoryLabel() {
    let label;
    if (this.enabled) {
      label = ContentBlocking.showBlockedLabels ? "contentBlocking.fingerprinters.blocking.label" : null;
    } else {
      label = ContentBlocking.showAllowedLabels ? "contentBlocking.fingerprinters.allowed.label" : null;
    }
    this.categoryLabel.textContent = label ? gNavigatorBundle.getString(label) : "";
  },

  isBlocking(state) {
    return (state & Ci.nsIWebProgressListener.STATE_BLOCKED_FINGERPRINTING_CONTENT) != 0;
  },

  isAllowing(state) {
    return this.enabled && (state & Ci.nsIWebProgressListener.STATE_LOADED_FINGERPRINTING_CONTENT) != 0;
  },

  isDetected(state) {
    return this.isBlocking(state) || this.isAllowing(state);
  },

  async updateSubView() {
    let contentBlockingLog = await gBrowser.selectedBrowser.getContentBlockingLog();
    contentBlockingLog = JSON.parse(contentBlockingLog);

    let fragment = document.createDocumentFragment();
    for (let [origin, actions] of Object.entries(contentBlockingLog)) {
      let listItem = this._createListItem(origin, actions);
      if (listItem) {
        fragment.appendChild(listItem);
      }
    }

    this.subViewList.textContent = "";
    this.subViewList.append(fragment);
  },

  _createListItem(origin, actions) {
    let isAllowed = actions.some(([state]) => this.isAllowing(state));
    let isDetected = isAllowed || actions.some(([state]) => this.isBlocking(state));

    if (!isDetected) {
      return null;
    }

    let uri = Services.io.newURI(origin);

    let listItem = document.createXULElement("hbox");
    listItem.className = "identity-popup-content-blocking-list-item";
    listItem.classList.toggle("allowed", isAllowed);
    // Repeat the host in the tooltip in case it's too long
    // and overflows in our panel.
    listItem.tooltipText = uri.host;

    let image = document.createXULElement("image");
    image.className = "identity-popup-fingerprintersView-icon";
    image.classList.toggle("allowed", isAllowed);
    listItem.append(image);

    let label = document.createXULElement("label");
    label.value = uri.host;
    label.className = "identity-popup-content-blocking-list-host-label";
    label.setAttribute("crop", "end");
    listItem.append(label);

    if (!isAllowed) {
      let stateLabel = document.createXULElement("label");
      stateLabel.value = this.strings.subViewBlocked;
      stateLabel.className = "identity-popup-content-blocking-list-state-label";
      listItem.append(stateLabel);
    }

    return listItem;
  },
};

var Cryptomining = {
  PREF_ENABLED: "privacy.trackingprotection.cryptomining.enabled",
  telemetryIdentifier: "cm",

  strings: {
    get subViewBlocked() {
      delete this.subViewBlocked;
      return this.subViewBlocked =
        gNavigatorBundle.getString("contentBlocking.cryptominersView.blocked.label");
    },
  },

  init() {
    XPCOMUtils.defineLazyPreferenceGetter(this, "enabled", this.PREF_ENABLED, false,
      () => this.updateCategoryLabel());
    this.updateCategoryLabel();
  },

  get categoryItem() {
    delete this.categoryItem;
    return this.categoryItem =
      document.getElementById("identity-popup-content-blocking-category-cryptominers");
  },

  get categoryLabel() {
    delete this.categoryLabel;
    return this.categoryLabel =
      document.getElementById("identity-popup-content-blocking-cryptominers-state-label");
  },

  get subViewList() {
    delete this.subViewList;
    return this.subViewList = document.getElementById("identity-popup-cryptominersView-list");
  },

  updateCategoryLabel() {
    let label;
    if (this.enabled) {
      label = ContentBlocking.showBlockedLabels ? "contentBlocking.cryptominers.blocking.label" : null;
    } else {
      label = ContentBlocking.showAllowedLabels ? "contentBlocking.cryptominers.allowed.label" : null;
    }
    this.categoryLabel.textContent = label ? gNavigatorBundle.getString(label) : "";
  },

  isBlocking(state) {
    return (state & Ci.nsIWebProgressListener.STATE_BLOCKED_CRYPTOMINING_CONTENT) != 0;
  },

  isAllowing(state) {
    return this.enabled && (state & Ci.nsIWebProgressListener.STATE_LOADED_CRYPTOMINING_CONTENT) != 0;
  },

  isDetected(state) {
    return this.isBlocking(state) || this.isAllowing(state);
  },

  async updateSubView() {
    let contentBlockingLog = await gBrowser.selectedBrowser.getContentBlockingLog();
    contentBlockingLog = JSON.parse(contentBlockingLog);

    let fragment = document.createDocumentFragment();
    for (let [origin, actions] of Object.entries(contentBlockingLog)) {
      let listItem = this._createListItem(origin, actions);
      if (listItem) {
        fragment.appendChild(listItem);
      }
    }

    this.subViewList.textContent = "";
    this.subViewList.append(fragment);
  },

  _createListItem(origin, actions) {
    let isAllowed = actions.some(([state]) => this.isAllowing(state));
    let isDetected = isAllowed || actions.some(([state]) => this.isBlocking(state));

    if (!isDetected) {
      return null;
    }

    let uri = Services.io.newURI(origin);

    let listItem = document.createXULElement("hbox");
    listItem.className = "identity-popup-content-blocking-list-item";
    listItem.classList.toggle("allowed", isAllowed);
    // Repeat the host in the tooltip in case it's too long
    // and overflows in our panel.
    listItem.tooltipText = uri.host;

    let image = document.createXULElement("image");
    image.className = "identity-popup-cryptominersView-icon";
    image.classList.toggle("allowed", isAllowed);
    listItem.append(image);

    let label = document.createXULElement("label");
    label.value = uri.host;
    label.className = "identity-popup-content-blocking-list-host-label";
    label.setAttribute("crop", "end");
    listItem.append(label);

    if (!isAllowed) {
      let stateLabel = document.createXULElement("label");
      stateLabel.value = this.strings.subViewBlocked;
      stateLabel.className = "identity-popup-content-blocking-list-state-label";
      listItem.append(stateLabel);
    }

    return listItem;
  },
};

var TrackingProtection = {
  reportBreakageLabel: "trackingprotection",
  telemetryIdentifier: "tp",
  PREF_ENABLED_GLOBALLY: "privacy.trackingprotection.enabled",
  PREF_ENABLED_IN_PRIVATE_WINDOWS: "privacy.trackingprotection.pbmode.enabled",
  PREF_TRACKING_TABLE: "urlclassifier.trackingTable",
  PREF_TRACKING_ANNOTATION_TABLE: "urlclassifier.trackingAnnotationTable",
  enabledGlobally: false,
  enabledInPrivateWindows: false,

  get categoryItem() {
    delete this.categoryItem;
    return this.categoryItem =
      document.getElementById("identity-popup-content-blocking-category-tracking-protection");
  },

  get categoryLabel() {
    delete this.categoryLabel;
    return this.categoryLabel =
      document.getElementById("identity-popup-content-blocking-tracking-protection-state-label");
  },

  get subViewList() {
    delete this.subViewList;
    return this.subViewList = document.getElementById("identity-popup-trackersView-list");
  },

  get strictInfo() {
    delete this.strictInfo;
    return this.strictInfo = document.getElementById("identity-popup-trackersView-strict-info");
  },

  strings: {
    get subViewBlocked() {
      delete this.subViewBlocked;
      return this.subViewBlocked =
        gNavigatorBundle.getString("contentBlocking.trackersView.blocked.label");
    },
  },

  init() {
    this.updateEnabled();

    Services.prefs.addObserver(this.PREF_ENABLED_GLOBALLY, this);
    Services.prefs.addObserver(this.PREF_ENABLED_IN_PRIVATE_WINDOWS, this);

    XPCOMUtils.defineLazyPreferenceGetter(this, "trackingTable", this.PREF_TRACKING_TABLE, false);
    XPCOMUtils.defineLazyPreferenceGetter(this, "trackingAnnotationTable", this.PREF_TRACKING_ANNOTATION_TABLE, false);
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
    this.updateCategoryLabel();
  },

  updateCategoryLabel() {
    let label;
    if (this.enabled) {
      label = ContentBlocking.showBlockedLabels ? "contentBlocking.trackers.blocking.label" : null;
    } else {
      label = ContentBlocking.showAllowedLabels ? "contentBlocking.trackers.allowed.label" : null;
    }
    this.categoryLabel.textContent = label ? gNavigatorBundle.getString(label) : "";
  },

  isBlocking(state) {
    return (state & Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT) != 0;
  },

  isAllowing(state) {
    return (state & Ci.nsIWebProgressListener.STATE_LOADED_TRACKING_CONTENT) != 0;
  },

  isDetected(state) {
    return this.isBlocking(state) || this.isAllowing(state);
  },

  async updateSubView() {
    let previousURI = gBrowser.currentURI.spec;
    let previousWindow = gBrowser.selectedBrowser.innerWindowID;

    let contentBlockingLog = await gBrowser.selectedBrowser.getContentBlockingLog();
    contentBlockingLog = JSON.parse(contentBlockingLog);

    // Don't tell the user to turn on TP if they are already blocking trackers.
    this.strictInfo.hidden = this.enabled;

    let fragment = document.createDocumentFragment();
    for (let [origin, actions] of Object.entries(contentBlockingLog)) {
      let listItem = await this._createListItem(origin, actions);
      if (listItem) {
        fragment.appendChild(listItem);
      }
    }

    // If we don't have trackers we would usually not show the menu item
    // allowing the user to show the sub-panel. However, in the edge case
    // that we annotated trackers on the page using the strict list but did
    // not detect trackers on the page using the basic list, we currently
    // still show the panel. To reduce the confusion, tell the user that we have
    // not detected any tracker.
    if (fragment.childNodes.length == 0) {
      let emptyBox = document.createXULElement("vbox");
      let emptyImage = document.createXULElement("image");
      emptyImage.classList.add("identity-popup-content-blocking-trackersView-empty-image");
      emptyImage.classList.add("tracking-protection-icon");

      let emptyLabel = document.createXULElement("label");
      emptyLabel.classList.add("identity-popup-content-blocking-empty-label");
      emptyLabel.textContent = gNavigatorBundle.getString("contentBlocking.trackersView.empty.label");

      emptyBox.appendChild(emptyImage);
      emptyBox.appendChild(emptyLabel);
      fragment.appendChild(emptyBox);

      this.subViewList.classList.add("empty");
    } else {
      this.subViewList.classList.remove("empty");
    }

    // This might have taken a while. Only update the list if we're still on the same page.
    if (previousURI == gBrowser.currentURI.spec &&
        previousWindow == gBrowser.selectedBrowser.innerWindowID) {
      this.subViewList.textContent = "";
      this.subViewList.append(fragment);
    }
  },

  // Given a URI from a source that was tracking-annotated, figure out
  // if it's really on the tracking table or just on the annotation table.
  _isOnTrackingTable(uri) {
    if (this.trackingTable == this.trackingAnnotationTable) {
      return true;
    }

    let feature = classifierService.getFeatureByName("tracking-protection");
    if (!feature) {
      return false;
    }

    return new Promise(resolve => {
      classifierService.asyncClassifyLocalWithFeatures(uri, [feature],
        Ci.nsIUrlClassifierFeature.blacklist,
        list => resolve(!!list.length));
    });
  },

  async _createListItem(origin, actions) {
    // Figure out if this list entry was actually detected by TP or something else.
    let isAllowed = actions.some(([state]) => this.isAllowing(state));
    let isDetected = isAllowed || actions.some(([state]) => this.isBlocking(state));

    if (!isDetected) {
      return null;
    }

    let uri = Services.io.newURI(origin);

    // Because we might use different lists for annotation vs. blocking, we
    // need to make sure that this is a tracker that we would actually have blocked
    // before showing it to the user.
    let isTracker = await this._isOnTrackingTable(uri);
    if (!isTracker) {
      return null;
    }

    let listItem = document.createXULElement("hbox");
    listItem.className = "identity-popup-content-blocking-list-item";
    listItem.classList.toggle("allowed", isAllowed);
    // Repeat the host in the tooltip in case it's too long
    // and overflows in our panel.
    listItem.tooltipText = uri.host;

    let image = document.createXULElement("image");
    image.className = "identity-popup-trackersView-icon";
    image.classList.toggle("allowed", isAllowed);
    listItem.append(image);

    let label = document.createXULElement("label");
    label.value = uri.host;
    label.className = "identity-popup-content-blocking-list-host-label";
    label.setAttribute("crop", "end");
    listItem.append(label);

    if (!isAllowed) {
      let stateLabel = document.createXULElement("label");
      stateLabel.value = this.strings.subViewBlocked;
      stateLabel.className = "identity-popup-content-blocking-list-state-label";
      listItem.append(stateLabel);
    }

    return listItem;
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

  get categoryItem() {
    delete this.categoryItem;
    return this.categoryItem =
      document.getElementById("identity-popup-content-blocking-category-cookies");
  },

  get categoryLabel() {
    delete this.categoryLabel;
    return this.categoryLabel =
      document.getElementById("identity-popup-content-blocking-cookies-state-label");
  },

  get subViewList() {
    delete this.subViewList;
    return this.subViewList = document.getElementById("identity-popup-cookiesView-list");
  },

  strings: {
    get subViewAllowed() {
      delete this.subViewAllowed;
      return this.subViewAllowed =
        gNavigatorBundle.getString("contentBlocking.cookiesView.allowed.label");
    },

    get subViewBlocked() {
      delete this.subViewBlocked;
      return this.subViewBlocked =
        gNavigatorBundle.getString("contentBlocking.cookiesView.blocked.label");
    },
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

  updateCategoryLabel() {
    let label;
    switch (this.behaviorPref) {
    case Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN:
      label = ContentBlocking.showBlockedLabels ? "contentBlocking.cookies.blocking3rdParty.label" : null;
      break;
    case Ci.nsICookieService.BEHAVIOR_REJECT:
      label = ContentBlocking.showBlockedLabels ? "contentBlocking.cookies.blockingAll.label" : null;
      break;
    case Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN:
      label = ContentBlocking.showBlockedLabels ? "contentBlocking.cookies.blockingUnvisited.label" : null;
      break;
    case Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER:
      label = ContentBlocking.showBlockedLabels ? "contentBlocking.cookies.blockingTrackers.label" : null;
      break;
    default:
      Cu.reportError(`Error: Unknown cookieBehavior pref observed: ${this.behaviorPref}`);
      // fall through
    case Ci.nsICookieService.BEHAVIOR_ACCEPT:
      label = ContentBlocking.showAllowedLabels ? "contentBlocking.cookies.allowed.label" : null;
      break;
    }
    this.categoryLabel.textContent = label ? gNavigatorBundle.getString(label) : "";
  },

  init() {
    XPCOMUtils.defineLazyPreferenceGetter(this, "behaviorPref", this.PREF_ENABLED,
      Ci.nsICookieService.BEHAVIOR_ACCEPT, this.updateCategoryLabel.bind(this));
    XPCOMUtils.defineLazyPreferenceGetter(this, "reportBreakageEnabled",
      this.PREF_REPORT_BREAKAGE_ENABLED, false);
    this.updateCategoryLabel();
  },

  get enabled() {
    return this.PREF_ENABLED_VALUES.includes(this.behaviorPref);
  },

  isBlocking(state) {
    return (state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER) != 0 ||
           (state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_ALL) != 0 ||
           (state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_BY_PERMISSION) != 0 ||
           (state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_FOREIGN) != 0;
  },

  isDetected(state) {
    return (state & Ci.nsIWebProgressListener.STATE_COOKIES_LOADED) != 0;
  },

  async updateSubView() {
    let contentBlockingLog = await gBrowser.selectedBrowser.getContentBlockingLog();
    contentBlockingLog = JSON.parse(contentBlockingLog);

    let categories = this._processContentBlockingLog(contentBlockingLog);

    this.subViewList.textContent = "";

    for (let category of ["firstParty", "trackers", "thirdParty"]) {
      let box = document.createXULElement("vbox");
      let label = document.createXULElement("label");
      label.className = "identity-popup-cookiesView-list-header";
      label.textContent = gNavigatorBundle.getString(`contentBlocking.cookiesView.${category}.label`);
      box.appendChild(label);

      for (let info of categories[category]) {
        box.appendChild(this._createListItem(info));
      }

      // If the category is empty, add a label noting that to the user.
      if (categories[category].length == 0) {
        let emptyLabel = document.createXULElement("label");
        emptyLabel.classList.add("identity-popup-content-blocking-empty-label");
        emptyLabel.textContent =
          gNavigatorBundle.getString(`contentBlocking.cookiesView.${category}.empty.label`);
        box.appendChild(emptyLabel);
      }

      this.subViewList.appendChild(box);
    }
  },

  _hasException(origin) {
    for (let perm of Services.perms.getAllForPrincipal(gBrowser.contentPrincipal)) {
      if (perm.type == "3rdPartyStorage^" + origin || perm.type.startsWith("3rdPartyStorage^" + origin + "^")) {
        return true;
      }
    }

    let principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(origin);
    // Cookie exceptions get "inherited" from parent- to sub-domain, so we need to
    // make sure to include parent domains in the permission check for "cookies".
    return Services.perms.testPermissionFromPrincipal(principal, "cookie") != Services.perms.UNKNOWN_ACTION;
  },

  _clearException(origin) {
    for (let perm of Services.perms.getAllForPrincipal(gBrowser.contentPrincipal)) {
      if (perm.type == "3rdPartyStorage^" + origin || perm.type.startsWith("3rdPartyStorage^" + origin + "^")) {
        Services.perms.removePermission(perm);
      }
    }

    // OAs don't matter here, so we can just use the hostname.
    let host = Services.io.newURI(origin).host;

    // Cookie exceptions get "inherited" from parent- to sub-domain, so we need to
    // clear any cookie permissions from parent domains as well.
    for (let perm of Services.perms.enumerator) {
      if (perm.type == "cookie" &&
          Services.eTLD.hasRootDomain(host, perm.principal.URI.host)) {
        Services.perms.removePermission(perm);
      }
    }
  },

  // Transforms and filters cookie entries in the content blocking log
  // so that we can categorize and display them in the UI.
  _processContentBlockingLog(log) {
    let newLog = {
      firstParty: [],
      trackers: [],
      thirdParty: [],
    };

    let firstPartyDomain = null;
    try {
      firstPartyDomain = Services.eTLD.getBaseDomain(gBrowser.currentURI);
    } catch (e) {
      // There are nasty edge cases here where someone is trying to set a cookie
      // on a public suffix or an IP address. Just categorize those as third party...
      if (e.result != Cr.NS_ERROR_HOST_IS_IP_ADDRESS &&
          e.result != Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
        throw e;
      }
    }

    for (let [origin, actions] of Object.entries(log)) {
      if (!origin.startsWith("http")) {
        continue;
      }

      let info = {origin, isAllowed: true, hasException: this._hasException(origin)};
      let hasCookie = false;
      let isTracker = false;

      // Extract information from the states entries in the content blocking log.
      // Each state will contain a single state flag from nsIWebProgressListener.
      // Note that we are using the same helper functions that are applied to the
      // bit map passed to onSecurityChange (which contains multiple states), thus
      // not checking exact equality, just presence of bits.
      for (let [state, blocked] of actions) {
        if (this.isDetected(state)) {
          hasCookie = true;
        }
        if (TrackingProtection.isAllowing(state)) {
          isTracker = true;
        }
        // blocked tells us whether the resource was actually blocked
        // (which it may not be in case of an exception).
        if (this.isBlocking(state)) {
          info.isAllowed = !blocked;
        }
      }

      if (!hasCookie) {
        continue;
      }

      let isFirstParty = false;
      try {
        let uri = Services.io.newURI(origin);
        isFirstParty = Services.eTLD.getBaseDomain(uri) == firstPartyDomain;
      } catch (e) {
        if (e.result != Cr.NS_ERROR_HOST_IS_IP_ADDRESS &&
            e.result != Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
          throw e;
        }
      }

      if (isFirstParty) {
        newLog.firstParty.push(info);
      } else if (isTracker) {
        newLog.trackers.push(info);
      } else {
        newLog.thirdParty.push(info);
      }
    }

    return newLog;
  },

  _createListItem({origin, isAllowed, hasException}) {
    let listItem = document.createXULElement("hbox");
    listItem.className = "identity-popup-content-blocking-list-item";
    listItem.classList.toggle("allowed", isAllowed);
    // Repeat the origin in the tooltip in case it's too long
    // and overflows in our panel.
    listItem.tooltipText = origin;

    let image = document.createXULElement("image");
    image.className = "identity-popup-cookiesView-icon";
    image.classList.toggle("allowed", isAllowed);
    listItem.append(image);

    let label = document.createXULElement("label");
    label.value = origin;
    label.className = "identity-popup-content-blocking-list-host-label";
    label.setAttribute("crop", "end");
    listItem.append(label);

    let stateLabel;
    if (isAllowed && hasException) {
      stateLabel = document.createXULElement("label");
      stateLabel.value = this.strings.subViewAllowed;
      stateLabel.className = "identity-popup-content-blocking-list-state-label";
      listItem.append(stateLabel);
    } else if (!isAllowed) {
      stateLabel = document.createXULElement("label");
      stateLabel.value = this.strings.subViewBlocked;
      stateLabel.className = "identity-popup-content-blocking-list-state-label";
      listItem.append(stateLabel);
    }

    if (hasException) {
      let removeException = document.createXULElement("button");
      removeException.className = "identity-popup-permission-remove-button";
      removeException.tooltipText = gNavigatorBundle.getFormattedString(
        "contentBlocking.cookiesView.removeButton.tooltip", [origin]);
      removeException.addEventListener("click", () => {
        this._clearException(origin);
        // Just flip the display based on what state we had previously.
        stateLabel.value = isAllowed ? this.strings.subViewBlocked : this.strings.subViewAllowed;
        listItem.classList.toggle("allowed", !isAllowed);
        image.classList.toggle("allowed", !isAllowed);
        removeException.hidden = true;
      });
      listItem.append(removeException);
    }

    return listItem;
  },
};


var ContentBlocking = {
  // If the user ignores the doorhanger, we stop showing it after some time.
  MAX_INTROS: 20,
  PREF_ANIMATIONS_ENABLED: "toolkit.cosmeticAnimations.enabled",
  PREF_REPORT_BREAKAGE_ENABLED: "browser.contentblocking.reportBreakage.enabled",
  PREF_REPORT_BREAKAGE_URL: "browser.contentblocking.reportBreakage.url",
  PREF_INTRO_COUNT_CB: "browser.contentblocking.introCount",
  PREF_CB_CATEGORY: "browser.contentblocking.category",
  PREF_SHOW_ALLOWED_LABELS: "browser.contentblocking.control-center.ui.showAllowedLabels",
  PREF_SHOW_BLOCKED_LABELS: "browser.contentblocking.control-center.ui.showBlockedLabels",

  get prefIntroCount() {
    return this.PREF_INTRO_COUNT_CB;
  },

  get content() {
    delete this.content;
    return this.content =
      document.getElementById("identity-popup-content-blocking-content");
  },

  get icon() {
    delete this.icon;
    return this.icon = document.getElementById("tracking-protection-icon");
  },

  get iconBox() {
    delete this.iconBox;
    return this.iconBox = document.getElementById("tracking-protection-icon-box");
  },

  get animatedIcon() {
    delete this.animatedIcon;
    return this.animatedIcon =
      document.getElementById("tracking-protection-icon-animatable-image");
  },

  get identityPopupMultiView() {
    delete this.identityPopupMultiView;
    return this.identityPopupMultiView =
      document.getElementById("identity-popup-multiView");
  },

  get reportBreakageButton() {
    delete this.reportBreakageButton;
    return this.reportBreakageButton =
      document.getElementById("identity-popup-content-blocking-report-breakage");
  },

  get reportBreakageURL() {
    delete this.reportBreakageURL;
    return this.reportBreakageURL =
      document.getElementById("identity-popup-breakageReportView-collection-url");
  },

  get reportBreakageLearnMore() {
    delete this.reportBreakageLearnMore;
    return this.reportBreakageLearnMore =
      document.getElementById("identity-popup-breakageReportView-learn-more");
  },

  get appMenuLabel() {
    delete this.appMenuLabel;
    return this.appMenuLabel = document.getElementById("appMenu-tp-label");
  },

  get identityPopup() {
    delete this.identityPopup;
    return this.identityPopup = document.getElementById("identity-popup");
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

    get activeTooltipText() {
      delete this.activeTooltipText;
      return this.activeTooltipText =
        gNavigatorBundle.getString("trackingProtection.icon.activeTooltip");
    },

    get disabledTooltipText() {
      delete this.disabledTooltipText;
      return this.disabledTooltipText =
        gNavigatorBundle.getString("trackingProtection.icon.disabledTooltip");
    },
  },

  // A list of blockers that will be displayed in the categories list
  // when blockable content is detected. A blocker must be an object
  // with at least the following two properties:
  //  - enabled: Whether the blocker is currently turned on.
  //  - isDetected(state): Given a content blocking state, whether the blocker has
  //                       either allowed or blocked elements.
  //  - categoryItem: The DOM item that represents the entry in the category list.
  //
  // It may also contain an init() and uninit() function, which will be called
  // on ContentBlocking.init() and ContentBlocking.uninit().
  blockers: [TrackingProtection, ThirdPartyCookies, Fingerprinting, Cryptomining],

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
    this.animatedIcon.addEventListener("animationend", () => this.iconBox.removeAttribute("animate"));

    let baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");
    this.reportBreakageLearnMore.href = baseURL + "blocking-breakage";

    this.updateAnimationsEnabled = () => {
      this.iconBox.toggleAttribute("animationsenabled",
        Services.prefs.getBoolPref(this.PREF_ANIMATIONS_ENABLED, false));
    };

    XPCOMUtils.defineLazyPreferenceGetter(this, "showBlockedLabels",
      this.PREF_SHOW_BLOCKED_LABELS, false, () => {
        for (let blocker of this.blockers) {
          blocker.updateCategoryLabel();
        }
    });
    XPCOMUtils.defineLazyPreferenceGetter(this, "showAllowedLabels",
      this.PREF_SHOW_ALLOWED_LABELS, false, () => {
        for (let blocker of this.blockers) {
          blocker.updateCategoryLabel();
        }
    });
    XPCOMUtils.defineLazyPreferenceGetter(this, "reportBreakageEnabled",
      this.PREF_REPORT_BREAKAGE_ENABLED, false);

    for (let blocker of this.blockers) {
      if (blocker.init) {
        blocker.init();
      }
    }

    this.updateAnimationsEnabled();

    Services.prefs.addObserver(this.PREF_ANIMATIONS_ENABLED, this.updateAnimationsEnabled);

    this.appMenuLabel.setAttribute("value", this.strings.appMenuTitle);
    this.appMenuLabel.setAttribute("tooltiptext", this.strings.appMenuTooltip);

    this.updateCBCategoryLabel = this.updateCBCategoryLabel.bind(this);
    this.updateCBCategoryLabel();
    Services.prefs.addObserver(this.PREF_CB_CATEGORY, this.updateCBCategoryLabel);
  },

  uninit() {
    for (let blocker of this.blockers) {
      if (blocker.uninit) {
        blocker.uninit();
      }
    }

    Services.prefs.removeObserver(this.PREF_ANIMATIONS_ENABLED, this.updateAnimationsEnabled);
    Services.prefs.removeObserver(this.PREF_CB_CATEGORY, this.updateCBCategoryLabel);
  },

  updateCBCategoryLabel() {
    if (!Services.prefs.prefHasUserValue(this.PREF_CB_CATEGORY)) {
      // Fallback to not setting a label, it's preferable to not set a label than to set an incorrect one.
      return;
    }
    let button = document.getElementById("tracking-protection-preferences-button");
    let appMenuCategoryLabel = document.getElementById("appMenu-tp-category");
    let label;
    let category = Services.prefs.getStringPref(this.PREF_CB_CATEGORY);
    switch (category) {
    case ("standard"):
      label = gNavigatorBundle.getString("contentBlocking.category.standard");
      break;
    case ("strict"):
      label = gNavigatorBundle.getString("contentBlocking.category.strict");
      break;
    case ("custom"):
      label = gNavigatorBundle.getString("contentBlocking.category.custom");
      break;
    }
    appMenuCategoryLabel.value = label;
    button.label = label;
  },

  hideIdentityPopupAndReload() {
    this.identityPopup.hidePopup();
    BrowserReload();
  },

  openPreferences(origin) {
    openPreferences("privacy-trackingprotection", { origin });
  },

  backToMainView() {
    this.identityPopupMultiView.goBack();
  },

  submitBreakageReport() {
    this.identityPopup.hidePopup();

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
    body += `urlclassifier.trackingTable: ${Services.prefs.getStringPref("urlclassifier.trackingTable")}\n`;
    body += `network.http.referer.defaultPolicy: ${Services.prefs.getIntPref("network.http.referer.defaultPolicy")}\n`;
    body += `network.http.referer.defaultPolicy.pbmode: ${Services.prefs.getIntPref("network.http.referer.defaultPolicy.pbmode")}\n`;
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
      } else {
        // Clear the textarea value when the report is submitted
        comments.value = "";
      }
    }).catch(Cu.reportError);
  },

  showReportBreakageSubview() {
    // Save this URI to make sure that the user really only submits the location
    // they see in the report breakage dialog.
    this.reportURI = gBrowser.currentURI;
    let urlWithoutQuery = this.reportURI.asciiSpec.replace("?" + this.reportURI.query, "");
    this.reportBreakageURL.value = urlWithoutQuery;
    this.identityPopupMultiView.showSubView("identity-popup-breakageReportView");
  },

  async showTrackersSubview() {
    await TrackingProtection.updateSubView();
    this.identityPopupMultiView.showSubView("identity-popup-trackersView");
  },

  async showCookiesSubview() {
    await ThirdPartyCookies.updateSubView();
    this.identityPopupMultiView.showSubView("identity-popup-cookiesView");
  },

  async showFingerprintersSubview() {
    await Fingerprinting.updateSubView();
    this.identityPopupMultiView.showSubView("identity-popup-fingerprintersView");
  },

  async showCryptominersSubview() {
    await Cryptomining.updateSubView();
    this.identityPopupMultiView.showSubView("identity-popup-cryptominersView");
  },

  shieldHistogramAdd(value) {
    if (PrivateBrowsingUtils.isWindowPrivate(window)) {
      return;
    }
    Services.telemetry.getHistogramById("TRACKING_PROTECTION_SHIELD").add(value);
  },

  onContentBlockingEvent(event, webProgress, isSimulated) {
    let baseURI = this._baseURIForChannelClassifier;

    // Don't deal with about:, file: etc.
    if (!baseURI) {
      this.iconBox.removeAttribute("animate");
      this.iconBox.removeAttribute("active");
      this.iconBox.removeAttribute("hasException");
      return;
    }

    let anyDetected = false;
    let anyBlocking = false;

    for (let blocker of this.blockers) {
      // Store data on whether the blocker is activated in the current document for
      // reporting it using the "report breakage" dialog. Under normal circumstances this
      // dialog should only be able to open in the currently selected tab and onSecurityChange
      // runs on tab switch, so we can avoid associating the data with the document directly.
      blocker.activated = blocker.isBlocking(event);
      blocker.categoryItem.classList.toggle("blocked", blocker.enabled);
      let detected = blocker.isDetected(event);
      blocker.categoryItem.hidden = !detected;
      anyDetected = anyDetected || detected;
      anyBlocking = anyBlocking || blocker.activated;
    }

    let isBrowserPrivate = PrivateBrowsingUtils.isBrowserPrivate(gBrowser.selectedBrowser);

    // Check whether the user has added an exception for this site.
    let type =  isBrowserPrivate ? "trackingprotection-pb" : "trackingprotection";
    let hasException = Services.perms.testExactPermission(baseURI, type) ==
      Services.perms.ALLOW_ACTION;

    // Reset the animation in case the user is switching tabs or if no blockers were detected
    // (this is most likely happening because the user navigated on to a different site). This
    // allows us to play it from the start without choppiness next time.
    if (isSimulated || !anyBlocking) {
      this.iconBox.removeAttribute("animate");
    // Only play the animation when the shield is not already shown on the page (the visibility
    // of the shield based on this onSecurityChange be determined afterwards).
    } else if (anyBlocking && !this.iconBox.hasAttribute("active")) {
      this.iconBox.setAttribute("animate", "true");

      if (!isBrowserPrivate) {
        let introCount = Services.prefs.getIntPref(this.prefIntroCount);
        let installStamp = Services.prefs.getIntPref(
          "browser.laterrun.bookkeeping.profileCreationTime", Date.now() / 1000);
        let delayLength = Services.prefs.getIntPref("browser.contentblocking.introDelaySeconds");
        let delayFulfilled = delayLength < (Date.now() / 1000 - installStamp);
        if (introCount < this.MAX_INTROS && delayFulfilled) {
          Services.prefs.setIntPref(this.prefIntroCount, ++introCount);
          Services.prefs.savePrefFile(null);
          this.showIntroPanel();
        }
      }
    }

    // We consider the shield state "active" when some kind of blocking activity
    // occurs on the page.  Note that merely allowing the loading of content that
    // we could have blocked does not trigger the appearance of the shield.
    // This state will be overriden later if there's an exception set for this site.
    this.content.toggleAttribute("detected", anyDetected);
    this.content.toggleAttribute("blocking", anyBlocking);
    this.content.toggleAttribute("hasException", hasException);

    this.iconBox.toggleAttribute("active", anyBlocking);
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

    if (hasException) {
      this.iconBox.setAttribute("tooltiptext", this.strings.disabledTooltipText);
      this.shieldHistogramAdd(1);
    } else if (anyBlocking) {
      this.iconBox.setAttribute("tooltiptext", this.strings.activeTooltipText);
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
