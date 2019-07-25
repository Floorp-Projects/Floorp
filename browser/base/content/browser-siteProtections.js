/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

ChromeUtils.defineModuleGetter(
  this,
  "ContentBlockingAllowList",
  "resource://gre/modules/ContentBlockingAllowList.jsm"
);

var Fingerprinting = {
  PREF_ENABLED: "privacy.trackingprotection.fingerprinting.enabled",
  reportBreakageLabel: "fingerprinting",

  strings: {
    get subViewBlocked() {
      delete this.subViewBlocked;
      return (this.subViewBlocked = gNavigatorBundle.getString(
        "contentBlocking.fingerprintersView.blocked.label"
      ));
    },
  },

  init() {
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "enabled",
      this.PREF_ENABLED,
      false,
      () => this.updateCategoryLabel()
    );
    this.updateCategoryLabel();
  },

  get categoryItem() {
    delete this.categoryItem;
    return (this.categoryItem = document.getElementById(
      "protections-popup-category-fingerprinters"
    ));
  },

  get categoryLabel() {
    delete this.categoryLabel;
    return (this.categoryLabel = document.getElementById(
      "protections-popup-fingerprinters-state-label"
    ));
  },

  get subViewList() {
    delete this.subViewList;
    return (this.subViewList = document.getElementById(
      "protections-popup-fingerprintersView-list"
    ));
  },

  updateCategoryLabel() {
    let label;
    if (this.enabled) {
      label = gProtectionsHandler.showBlockedLabels
        ? "contentBlocking.fingerprinters.blocking.label"
        : null;
    } else {
      label = gProtectionsHandler.showAllowedLabels
        ? "contentBlocking.fingerprinters.allowed.label"
        : null;
    }
    this.categoryLabel.textContent = label
      ? gNavigatorBundle.getString(label)
      : "";
  },

  isBlocking(state) {
    return (
      (state &
        Ci.nsIWebProgressListener.STATE_BLOCKED_FINGERPRINTING_CONTENT) !=
      0
    );
  },

  isAllowing(state) {
    return (
      this.enabled &&
      (state & Ci.nsIWebProgressListener.STATE_LOADED_FINGERPRINTING_CONTENT) !=
        0
    );
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
    let isDetected =
      isAllowed || actions.some(([state]) => this.isBlocking(state));

    if (!isDetected) {
      return null;
    }

    let uri = Services.io.newURI(origin);

    let listItem = document.createXULElement("hbox");
    listItem.className = "protections-popup-list-item";
    listItem.classList.toggle("allowed", isAllowed);
    // Repeat the host in the tooltip in case it's too long
    // and overflows in our panel.
    listItem.tooltipText = uri.host;

    let image = document.createXULElement("image");
    image.className = "protections-popup-fingerprintersView-icon";
    image.classList.toggle("allowed", isAllowed);
    listItem.append(image);

    let label = document.createXULElement("label");
    label.value = uri.host;
    label.className = "protections-popup-list-host-label";
    label.setAttribute("crop", "end");
    listItem.append(label);

    if (!isAllowed) {
      let stateLabel = document.createXULElement("label");
      stateLabel.value = this.strings.subViewBlocked;
      stateLabel.className = "protections-popup-list-state-label";
      listItem.append(stateLabel);
    }

    return listItem;
  },
};

var Cryptomining = {
  PREF_ENABLED: "privacy.trackingprotection.cryptomining.enabled",
  reportBreakageLabel: "cryptomining",

  strings: {
    get subViewBlocked() {
      delete this.subViewBlocked;
      return (this.subViewBlocked = gNavigatorBundle.getString(
        "contentBlocking.cryptominersView.blocked.label"
      ));
    },
  },

  init() {
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "enabled",
      this.PREF_ENABLED,
      false,
      () => this.updateCategoryLabel()
    );
    this.updateCategoryLabel();
  },

  get categoryItem() {
    delete this.categoryItem;
    return (this.categoryItem = document.getElementById(
      "protections-popup-category-cryptominers"
    ));
  },

  get categoryLabel() {
    delete this.categoryLabel;
    return (this.categoryLabel = document.getElementById(
      "protections-popup-cryptominers-state-label"
    ));
  },

  get subViewList() {
    delete this.subViewList;
    return (this.subViewList = document.getElementById(
      "protections-popup-cryptominersView-list"
    ));
  },

  updateCategoryLabel() {
    let label;
    if (this.enabled) {
      label = gProtectionsHandler.showBlockedLabels
        ? "contentBlocking.cryptominers.blocking.label"
        : null;
    } else {
      label = gProtectionsHandler.showAllowedLabels
        ? "contentBlocking.cryptominers.allowed.label"
        : null;
    }
    this.categoryLabel.textContent = label
      ? gNavigatorBundle.getString(label)
      : "";
  },

  isBlocking(state) {
    return (
      (state & Ci.nsIWebProgressListener.STATE_BLOCKED_CRYPTOMINING_CONTENT) !=
      0
    );
  },

  isAllowing(state) {
    return (
      this.enabled &&
      (state & Ci.nsIWebProgressListener.STATE_LOADED_CRYPTOMINING_CONTENT) != 0
    );
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
    let isDetected =
      isAllowed || actions.some(([state]) => this.isBlocking(state));

    if (!isDetected) {
      return null;
    }

    let uri = Services.io.newURI(origin);

    let listItem = document.createXULElement("hbox");
    listItem.className = "protections-popup-list-item";
    listItem.classList.toggle("allowed", isAllowed);
    // Repeat the host in the tooltip in case it's too long
    // and overflows in our panel.
    listItem.tooltipText = uri.host;

    let image = document.createXULElement("image");
    image.className = "protections-popup-cryptominersView-icon";
    image.classList.toggle("allowed", isAllowed);
    listItem.append(image);

    let label = document.createXULElement("label");
    label.value = uri.host;
    label.className = "protections-popup-list-host-label";
    label.setAttribute("crop", "end");
    listItem.append(label);

    if (!isAllowed) {
      let stateLabel = document.createXULElement("label");
      stateLabel.value = this.strings.subViewBlocked;
      stateLabel.className = "protections-popup-list-state-label";
      listItem.append(stateLabel);
    }

    return listItem;
  },
};

var TrackingProtection = {
  reportBreakageLabel: "trackingprotection",
  PREF_ENABLED_GLOBALLY: "privacy.trackingprotection.enabled",
  PREF_ENABLED_IN_PRIVATE_WINDOWS: "privacy.trackingprotection.pbmode.enabled",
  PREF_TRACKING_TABLE: "urlclassifier.trackingTable",
  PREF_TRACKING_ANNOTATION_TABLE: "urlclassifier.trackingAnnotationTable",
  enabledGlobally: false,
  enabledInPrivateWindows: false,

  get categoryItem() {
    delete this.categoryItem;
    return (this.categoryItem = document.getElementById(
      "protections-popup-category-tracking-protection"
    ));
  },

  get categoryLabel() {
    delete this.categoryLabel;
    return (this.categoryLabel = document.getElementById(
      "protections-popup-tracking-protection-state-label"
    ));
  },

  get subViewList() {
    delete this.subViewList;
    return (this.subViewList = document.getElementById(
      "protections-popup-trackersView-list"
    ));
  },

  get strictInfo() {
    delete this.strictInfo;
    return (this.strictInfo = document.getElementById(
      "protections-popup-trackersView-strict-info"
    ));
  },

  strings: {
    get subViewBlocked() {
      delete this.subViewBlocked;
      return (this.subViewBlocked = gNavigatorBundle.getString(
        "contentBlocking.trackersView.blocked.label"
      ));
    },
  },

  init() {
    this.updateEnabled();

    Services.prefs.addObserver(this.PREF_ENABLED_GLOBALLY, this);
    Services.prefs.addObserver(this.PREF_ENABLED_IN_PRIVATE_WINDOWS, this);

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "trackingTable",
      this.PREF_TRACKING_TABLE,
      false
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "trackingAnnotationTable",
      this.PREF_TRACKING_ANNOTATION_TABLE,
      false
    );
  },

  uninit() {
    Services.prefs.removeObserver(this.PREF_ENABLED_GLOBALLY, this);
    Services.prefs.removeObserver(this.PREF_ENABLED_IN_PRIVATE_WINDOWS, this);
  },

  observe() {
    this.updateEnabled();
  },

  get enabled() {
    return (
      this.enabledGlobally ||
      (this.enabledInPrivateWindows &&
        PrivateBrowsingUtils.isWindowPrivate(window))
    );
  },

  updateEnabled() {
    this.enabledGlobally = Services.prefs.getBoolPref(
      this.PREF_ENABLED_GLOBALLY
    );
    this.enabledInPrivateWindows = Services.prefs.getBoolPref(
      this.PREF_ENABLED_IN_PRIVATE_WINDOWS
    );
    this.updateCategoryLabel();
  },

  updateCategoryLabel() {
    let label;
    if (this.enabled) {
      label = gProtectionsHandler.showBlockedLabels
        ? "contentBlocking.trackers.blocking.label"
        : null;
    } else {
      label = gProtectionsHandler.showAllowedLabels
        ? "contentBlocking.trackers.allowed.label"
        : null;
    }
    this.categoryLabel.textContent = label
      ? gNavigatorBundle.getString(label)
      : "";
  },

  isBlocking(state) {
    return (
      (state & Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT) != 0
    );
  },

  isAllowing(state) {
    return (
      (state & Ci.nsIWebProgressListener.STATE_LOADED_TRACKING_CONTENT) != 0
    );
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
      emptyImage.classList.add("protections-popup-trackersView-empty-image");
      emptyImage.classList.add("tracking-protection-icon");

      let emptyLabel = document.createXULElement("label");
      emptyLabel.classList.add("protections-popup-empty-label");
      emptyLabel.textContent = gNavigatorBundle.getString(
        "contentBlocking.trackersView.empty.label"
      );

      emptyBox.appendChild(emptyImage);
      emptyBox.appendChild(emptyLabel);
      fragment.appendChild(emptyBox);

      this.subViewList.classList.add("empty");
    } else {
      this.subViewList.classList.remove("empty");
    }

    // This might have taken a while. Only update the list if we're still on the same page.
    if (
      previousURI == gBrowser.currentURI.spec &&
      previousWindow == gBrowser.selectedBrowser.innerWindowID
    ) {
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
      classifierService.asyncClassifyLocalWithFeatures(
        uri,
        [feature],
        Ci.nsIUrlClassifierFeature.blacklist,
        list => resolve(!!list.length)
      );
    });
  },

  async _createListItem(origin, actions) {
    // Figure out if this list entry was actually detected by TP or something else.
    let isAllowed = actions.some(([state]) => this.isAllowing(state));
    let isDetected =
      isAllowed || actions.some(([state]) => this.isBlocking(state));

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
    listItem.className = "protections-popup-list-item";
    listItem.classList.toggle("allowed", isAllowed);
    // Repeat the host in the tooltip in case it's too long
    // and overflows in our panel.
    listItem.tooltipText = uri.host;

    let image = document.createXULElement("image");
    image.className = "protections-popup-trackersView-icon";
    image.classList.toggle("allowed", isAllowed);
    listItem.append(image);

    let label = document.createXULElement("label");
    label.value = uri.host;
    label.className = "protections-popup-list-host-label";
    label.setAttribute("crop", "end");
    listItem.append(label);

    if (!isAllowed) {
      let stateLabel = document.createXULElement("label");
      stateLabel.value = this.strings.subViewBlocked;
      stateLabel.className = "protections-popup-list-state-label";
      listItem.append(stateLabel);
    }

    return listItem;
  },
};

var ThirdPartyCookies = {
  PREF_ENABLED: "network.cookie.cookieBehavior",
  PREF_ENABLED_VALUES: [
    // These values match the ones exposed under the Content Blocking section
    // of the Preferences UI.
    Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN, // Block all third-party cookies
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER, // Block third-party cookies from trackers
  ],

  get categoryItem() {
    delete this.categoryItem;
    return (this.categoryItem = document.getElementById(
      "protections-popup-category-cookies"
    ));
  },

  get categoryLabel() {
    delete this.categoryLabel;
    return (this.categoryLabel = document.getElementById(
      "protections-popup-cookies-state-label"
    ));
  },

  get subViewList() {
    delete this.subViewList;
    return (this.subViewList = document.getElementById(
      "protections-popup-cookiesView-list"
    ));
  },

  strings: {
    get subViewAllowed() {
      delete this.subViewAllowed;
      return (this.subViewAllowed = gNavigatorBundle.getString(
        "contentBlocking.cookiesView.allowed.label"
      ));
    },

    get subViewBlocked() {
      delete this.subViewBlocked;
      return (this.subViewBlocked = gNavigatorBundle.getString(
        "contentBlocking.cookiesView.blocked.label"
      ));
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
        Cu.reportError(
          `Error: Unknown cookieBehavior pref observed: ${this.behaviorPref}`
        );
      // fall through
      case Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER:
        return "cookierestrictions";
      case Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN:
        return "cookierestrictionsforeignpartitioned";
    }
  },

  updateCategoryLabel() {
    let label;
    switch (this.behaviorPref) {
      case Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN:
        label = gProtectionsHandler.showBlockedLabels
          ? "contentBlocking.cookies.blocking3rdParty.label"
          : null;
        break;
      case Ci.nsICookieService.BEHAVIOR_REJECT:
        label = gProtectionsHandler.showBlockedLabels
          ? "contentBlocking.cookies.blockingAll.label"
          : null;
        break;
      case Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN:
        label = gProtectionsHandler.showBlockedLabels
          ? "contentBlocking.cookies.blockingUnvisited.label"
          : null;
        break;
      case Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER:
        label = gProtectionsHandler.showBlockedLabels
          ? "contentBlocking.cookies.blockingTrackers.label"
          : null;
        break;
      default:
        Cu.reportError(
          `Error: Unknown cookieBehavior pref observed: ${this.behaviorPref}`
        );
      // fall through
      case Ci.nsICookieService.BEHAVIOR_ACCEPT:
        label = gProtectionsHandler.showAllowedLabels
          ? "contentBlocking.cookies.allowed.label"
          : null;
        break;
    }
    this.categoryLabel.textContent = label
      ? gNavigatorBundle.getString(label)
      : "";
  },

  init() {
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "behaviorPref",
      this.PREF_ENABLED,
      Ci.nsICookieService.BEHAVIOR_ACCEPT,
      this.updateCategoryLabel.bind(this)
    );
    this.updateCategoryLabel();
  },

  get enabled() {
    return this.PREF_ENABLED_VALUES.includes(this.behaviorPref);
  },

  isBlocking(state) {
    return (
      (state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER) != 0 ||
      (state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_ALL) != 0 ||
      (state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_BY_PERMISSION) !=
        0 ||
      (state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_FOREIGN) != 0
    );
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
      label.className = "protections-popup-cookiesView-list-header";
      label.textContent = gNavigatorBundle.getString(
        `contentBlocking.cookiesView.${category}.label`
      );
      box.appendChild(label);

      for (let info of categories[category]) {
        box.appendChild(this._createListItem(info));
      }

      // If the category is empty, add a label noting that to the user.
      if (categories[category].length == 0) {
        let emptyLabel = document.createXULElement("label");
        emptyLabel.classList.add("protections-popup-empty-label");
        emptyLabel.textContent = gNavigatorBundle.getString(
          `contentBlocking.cookiesView.${category}.empty.label`
        );
        box.appendChild(emptyLabel);
      }

      this.subViewList.appendChild(box);
    }
  },

  _hasException(origin) {
    for (let perm of Services.perms.getAllForPrincipal(
      gBrowser.contentPrincipal
    )) {
      if (
        perm.type == "3rdPartyStorage^" + origin ||
        perm.type.startsWith("3rdPartyStorage^" + origin + "^")
      ) {
        return true;
      }
    }

    let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      origin
    );
    // Cookie exceptions get "inherited" from parent- to sub-domain, so we need to
    // make sure to include parent domains in the permission check for "cookies".
    return (
      Services.perms.testPermissionFromPrincipal(principal, "cookie") !=
      Services.perms.UNKNOWN_ACTION
    );
  },

  _clearException(origin) {
    for (let perm of Services.perms.getAllForPrincipal(
      gBrowser.contentPrincipal
    )) {
      if (
        perm.type == "3rdPartyStorage^" + origin ||
        perm.type.startsWith("3rdPartyStorage^" + origin + "^")
      ) {
        Services.perms.removePermission(perm);
      }
    }

    // OAs don't matter here, so we can just use the hostname.
    let host = Services.io.newURI(origin).host;

    // Cookie exceptions get "inherited" from parent- to sub-domain, so we need to
    // clear any cookie permissions from parent domains as well.
    for (let perm of Services.perms.enumerator) {
      if (
        perm.type == "cookie" &&
        Services.eTLD.hasRootDomain(host, perm.principal.URI.host)
      ) {
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
      if (
        e.result != Cr.NS_ERROR_HOST_IS_IP_ADDRESS &&
        e.result != Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS
      ) {
        throw e;
      }
    }

    for (let [origin, actions] of Object.entries(log)) {
      if (!origin.startsWith("http")) {
        continue;
      }

      let info = {
        origin,
        isAllowed: true,
        hasException: this._hasException(origin),
      };
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
        if (
          e.result != Cr.NS_ERROR_HOST_IS_IP_ADDRESS &&
          e.result != Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS
        ) {
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

  _createListItem({ origin, isAllowed, hasException }) {
    let listItem = document.createXULElement("hbox");
    listItem.className = "protections-popup-list-item";
    listItem.classList.toggle("allowed", isAllowed);
    // Repeat the origin in the tooltip in case it's too long
    // and overflows in our panel.
    listItem.tooltipText = origin;

    let image = document.createXULElement("image");
    image.className = "protections-popup-cookiesView-icon";
    image.classList.toggle("allowed", isAllowed);
    listItem.append(image);

    let label = document.createXULElement("label");
    label.value = origin;
    label.className = "protections-popup-list-host-label";
    label.setAttribute("crop", "end");
    listItem.append(label);

    let stateLabel;
    if (isAllowed && hasException) {
      stateLabel = document.createXULElement("label");
      stateLabel.value = this.strings.subViewAllowed;
      stateLabel.className = "protections-popup-list-state-label";
      listItem.append(stateLabel);
    } else if (!isAllowed) {
      stateLabel = document.createXULElement("label");
      stateLabel.value = this.strings.subViewBlocked;
      stateLabel.className = "protections-popup-list-state-label";
      listItem.append(stateLabel);
    }

    if (hasException) {
      let removeException = document.createXULElement("button");
      removeException.className = "identity-popup-permission-remove-button";
      removeException.tooltipText = gNavigatorBundle.getFormattedString(
        "contentBlocking.cookiesView.removeButton.tooltip",
        [origin]
      );
      removeException.addEventListener("click", () => {
        this._clearException(origin);
        // Just flip the display based on what state we had previously.
        stateLabel.value = isAllowed
          ? this.strings.subViewBlocked
          : this.strings.subViewAllowed;
        listItem.classList.toggle("allowed", !isAllowed);
        image.classList.toggle("allowed", !isAllowed);
        removeException.hidden = true;
      });
      listItem.append(removeException);
    }

    return listItem;
  },
};

/**
 * Utility object to handle manipulations of the protections indicators in the UI
 */
var gProtectionsHandler = {
  PREF_ANIMATIONS_ENABLED: "toolkit.cosmeticAnimations.enabled",
  PREF_REPORT_BREAKAGE_URL: "browser.contentblocking.reportBreakage.url",
  PREF_CB_CATEGORY: "browser.contentblocking.category",
  PREF_SHOW_ALLOWED_LABELS:
    "browser.contentblocking.control-center.ui.showAllowedLabels",
  PREF_SHOW_BLOCKED_LABELS:
    "browser.contentblocking.control-center.ui.showBlockedLabels",

  // smart getters
  get _protectionsPopup() {
    delete this._protectionsPopup;
    return (this._protectionsPopup = document.getElementById(
      "protections-popup"
    ));
  },
  get iconBox() {
    delete this.iconBox;
    return (this.iconBox = document.getElementById(
      "tracking-protection-icon-box"
    ));
  },
  get animatedIcon() {
    delete this.animatedIcon;
    return (this.animatedIcon = document.getElementById(
      "tracking-protection-icon-animatable-image"
    ));
  },
  get appMenuLabel() {
    delete this.appMenuLabel;
    return (this.appMenuLabel = document.getElementById("appMenu-tp-label"));
  },
  get _protectionsIconBox() {
    delete this._protectionsIconBox;
    return (this._protectionsIconBox = document.getElementById(
      "tracking-protection-icon-animatable-box"
    ));
  },
  get _protectionsPopupMultiView() {
    delete this._protectionsPopupMultiView;
    return (this._protectionsPopupMultiView = document.getElementById(
      "protections-popup-multiView"
    ));
  },
  get _protectionsPopupMainView() {
    delete this._protectionsPopupMainView;
    return (this._protectionsPopupMainView = document.getElementById(
      "protections-popup-mainView"
    ));
  },
  get _protectionsPopupMainViewHeaderLabel() {
    delete this._protectionsPopupMainViewHeaderLabel;
    return (this._protectionsPopupMainViewHeaderLabel = document.getElementById(
      "protections-popup-mainView-panel-header-span"
    ));
  },
  get _protectionsPopupTPSwitchBreakageLink() {
    delete this._protectionsPopupTPSwitchBreakageLink;
    return (this._protectionsPopupTPSwitchBreakageLink = document.getElementById(
      "protections-popup-tp-switch-breakage-link"
    ));
  },
  get _protectionsPopupTPSwitchSection() {
    delete this._protectionsPopupTPSwitchSection;
    return (this._protectionsPopupTPSwitchSection = document.getElementById(
      "protections-popup-tp-switch-section"
    ));
  },
  get _protectionsPopupTPSwitch() {
    delete this._protectionsPopupTPSwitch;
    return (this._protectionsPopupTPSwitch = document.getElementById(
      "protections-popup-tp-switch"
    ));
  },
  get _protectionPopupSettingsButton() {
    delete this._protectionPopupSettingsButton;
    return (this._protectionPopupSettingsButton = document.getElementById(
      "protections-popup-settings-button"
    ));
  },
  get _protectionPopupFooter() {
    delete this._protectionPopupFooter;
    return (this._protectionPopupFooter = document.getElementById(
      "protections-popup-footer"
    ));
  },
  get _protectionPopupTrackersCounterDescription() {
    delete this._protectionPopupTrackersCounterDescription;
    return (this._protectionPopupTrackersCounterDescription = document.getElementById(
      "protections-popup-trackers-blocked-counter-description"
    ));
  },
  get _protectionsPopupSiteNotWorkingTPSwitch() {
    delete this._protectionsPopupSiteNotWorkingTPSwitch;
    return (this._protectionsPopupSiteNotWorkingTPSwitch = document.getElementById(
      "protections-popup-siteNotWorking-tp-switch"
    ));
  },
  get _protectionsPopupSendReportLearnMore() {
    delete this._protectionsPopupSendReportLearnMore;
    return (this._protectionsPopupSendReportLearnMore = document.getElementById(
      "protections-popup-sendReportView-learn-more"
    ));
  },
  get _protectionsPopupSendReportURL() {
    delete this._protectionsPopupSendReportURL;
    return (this._protectionsPopupSendReportURL = document.getElementById(
      "protections-popup-sendReportView-collection-url"
    ));
  },

  strings: {
    get appMenuTitle() {
      delete this.appMenuTitle;
      return (this.appMenuTitle = gNavigatorBundle.getString(
        "contentBlocking.title"
      ));
    },

    get appMenuTooltip() {
      delete this.appMenuTooltip;
      if (AppConstants.platform == "win") {
        return (this.appMenuTooltip = gNavigatorBundle.getString(
          "contentBlocking.tooltipWin"
        ));
      }
      return (this.appMenuTooltip = gNavigatorBundle.getString(
        "contentBlocking.tooltipOther"
      ));
    },

    get activeTooltipText() {
      delete this.activeTooltipText;
      return (this.activeTooltipText = gNavigatorBundle.getString(
        "trackingProtection.icon.activeTooltip"
      ));
    },

    get disabledTooltipText() {
      delete this.disabledTooltipText;
      return (this.disabledTooltipText = gNavigatorBundle.getString(
        "trackingProtection.icon.disabledTooltip"
      ));
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
  // on gProtectionsHandler.init() and gProtectionsHandler.uninit().
  blockers: [
    TrackingProtection,
    ThirdPartyCookies,
    Fingerprinting,
    Cryptomining,
  ],

  init() {
    this.animatedIcon.addEventListener("animationend", () =>
      this.iconBox.removeAttribute("animate")
    );

    this.updateAnimationsEnabled = () => {
      this.iconBox.toggleAttribute(
        "animationsenabled",
        Services.prefs.getBoolPref(this.PREF_ANIMATIONS_ENABLED, false)
      );
    };

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_protectionsPopupToastTimeout",
      "browser.protections_panel.toast.timeout",
      5000
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "showBlockedLabels",
      this.PREF_SHOW_BLOCKED_LABELS,
      false,
      () => {
        for (let blocker of this.blockers) {
          blocker.updateCategoryLabel();
        }
      }
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "showAllowedLabels",
      this.PREF_SHOW_ALLOWED_LABELS,
      false,
      () => {
        for (let blocker of this.blockers) {
          blocker.updateCategoryLabel();
        }
      }
    );

    for (let blocker of this.blockers) {
      if (blocker.init) {
        blocker.init();
      }
    }

    this.updateAnimationsEnabled();

    Services.prefs.addObserver(
      this.PREF_ANIMATIONS_ENABLED,
      this.updateAnimationsEnabled
    );

    let baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");
    gProtectionsHandler._protectionsPopupSendReportLearnMore.href =
      baseURL + "blocking-breakage";

    this.appMenuLabel.setAttribute("value", this.strings.appMenuTitle);
    this.appMenuLabel.setAttribute("tooltiptext", this.strings.appMenuTooltip);

    this.updateCBCategoryLabel = this.updateCBCategoryLabel.bind(this);
    this.updateCBCategoryLabel();
    Services.prefs.addObserver(
      this.PREF_CB_CATEGORY,
      this.updateCBCategoryLabel
    );
  },

  uninit() {
    for (let blocker of this.blockers) {
      if (blocker.uninit) {
        blocker.uninit();
      }
    }

    Services.prefs.removeObserver(
      this.PREF_ANIMATIONS_ENABLED,
      this.updateAnimationsEnabled
    );
    Services.prefs.removeObserver(
      this.PREF_CB_CATEGORY,
      this.updateCBCategoryLabel
    );
  },

  updateCBCategoryLabel() {
    if (!Services.prefs.prefHasUserValue(this.PREF_CB_CATEGORY)) {
      // Fallback to not setting a label, it's preferable to not set a label than to set an incorrect one.
      return;
    }
    let appMenuCategoryLabel = document.getElementById("appMenu-tp-category");
    let label;
    let category = Services.prefs.getStringPref(this.PREF_CB_CATEGORY);
    switch (category) {
      case "standard":
        label = gNavigatorBundle.getString("contentBlocking.category.standard");
        break;
      case "strict":
        label = gNavigatorBundle.getString("contentBlocking.category.strict");
        break;
      case "custom":
        label = gNavigatorBundle.getString("contentBlocking.category.custom");
        break;
    }
    appMenuCategoryLabel.value = label;
  },

  openPreferences(origin) {
    openPreferences("privacy-trackingprotection", { origin });
  },

  async showTrackersSubview() {
    await TrackingProtection.updateSubView();
    this._protectionsPopupMultiView.showSubView(
      "protections-popup-trackersView"
    );
  },

  async showCookiesSubview() {
    await ThirdPartyCookies.updateSubView();
    this._protectionsPopupMultiView.showSubView(
      "protections-popup-cookiesView"
    );
  },

  async showFingerprintersSubview() {
    await Fingerprinting.updateSubView();
    this._protectionsPopupMultiView.showSubView(
      "protections-popup-fingerprintersView"
    );
  },

  async showCryptominersSubview() {
    await Cryptomining.updateSubView();
    this._protectionsPopupMultiView.showSubView(
      "protections-popup-cryptominersView"
    );
  },

  shieldHistogramAdd(value) {
    if (PrivateBrowsingUtils.isWindowPrivate(window)) {
      return;
    }
    Services.telemetry
      .getHistogramById("TRACKING_PROTECTION_SHIELD")
      .add(value);
  },

  cryptominersHistogramAdd(value) {
    Services.telemetry
      .getHistogramById("CRYPTOMINERS_BLOCKED_COUNT")
      .add(value);
  },

  fingerprintersHistogramAdd(value) {
    Services.telemetry
      .getHistogramById("FINGERPRINTERS_BLOCKED_COUNT")
      .add(value);
  },

  handleProtectionsButtonEvent(event) {
    event.stopPropagation();
    if (
      (event.type == "click" && event.button != 0) ||
      (event.type == "keypress" &&
        event.charCode != KeyEvent.DOM_VK_SPACE &&
        event.keyCode != KeyEvent.DOM_VK_RETURN)
    ) {
      return; // Left click, space or enter only
    }

    this.showProtectionsPopup({ event });
  },

  onPopupShown(event) {
    if (event.target == this._protectionsPopup) {
      window.addEventListener("focus", this, true);

      // Add the "open" attribute to the tracking protection icon container
      // for styling.
      gIdentityHandler._trackingProtectionIconContainer.setAttribute(
        "open",
        "true"
      );
    }
  },

  onPopupHidden(event) {
    if (event.target == this._protectionsPopup) {
      window.removeEventListener("focus", this, true);
      gIdentityHandler._trackingProtectionIconContainer.removeAttribute("open");
    }
  },

  onHeaderClicked(event) {
    // Display the whole protections panel if the toast has been clicked.
    if (this._protectionsPopup.hasAttribute("toast")) {
      // Hide the toast first.
      PanelMultiView.hidePopup(this._protectionsPopup);

      // Open the full protections panel.
      this.showProtectionsPopup({ event });
    }
  },

  // This triggers from top level location changes.
  onLocationChange() {
    if (this._showToastAfterRefresh) {
      this._showToastAfterRefresh = false;

      // We only display the toast if we're still on the same page.
      if (
        this._previousURI == gBrowser.currentURI.spec &&
        this._previousOuterWindowID == gBrowser.selectedBrowser.outerWindowID
      ) {
        this.showProtectionsPopup({
          toast: true,
        });
      }
    }

    // Reset blocking and exception status so that we can send telemetry
    this.hadShieldState = false;

    // Don't deal with about:, file: etc.
    if (!ContentBlockingAllowList.canHandle(gBrowser.selectedBrowser)) {
      return;
    }

    // Check whether the user has added an exception for this site.
    let hasException = ContentBlockingAllowList.includes(
      gBrowser.selectedBrowser
    );

    this._protectionsPopup.toggleAttribute("hasException", hasException);
    this.iconBox.toggleAttribute("hasException", hasException);

    // Add to telemetry per page load as a baseline measurement.
    this.fingerprintersHistogramAdd("pageLoad");
    this.cryptominersHistogramAdd("pageLoad");
    this.shieldHistogramAdd(0);
  },

  onContentBlockingEvent(event, webProgress, isSimulated) {
    let previousState = gBrowser.securityUI.contentBlockingEvent;

    // Don't deal with about:, file: etc.
    if (!ContentBlockingAllowList.canHandle(gBrowser.selectedBrowser)) {
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

    // Check whether the user has added an exception for this site.
    let hasException = ContentBlockingAllowList.includes(
      gBrowser.selectedBrowser
    );

    // Reset the animation in case the user is switching tabs or if no blockers were detected
    // (this is most likely happening because the user navigated on to a different site). This
    // allows us to play it from the start without choppiness next time.
    if (isSimulated || !anyBlocking) {
      this.iconBox.removeAttribute("animate");
      // Only play the animation when the shield is not already shown on the page (the visibility
      // of the shield based on this onSecurityChange be determined afterwards).
    } else if (anyBlocking && !this.iconBox.hasAttribute("active")) {
      this.iconBox.setAttribute("animate", "true");
    }

    // We consider the shield state "active" when some kind of blocking activity
    // occurs on the page.  Note that merely allowing the loading of content that
    // we could have blocked does not trigger the appearance of the shield.
    // This state will be overriden later if there's an exception set for this site.
    this._protectionsPopup.toggleAttribute("detected", anyDetected);
    this._protectionsPopup.toggleAttribute("blocking", anyBlocking);
    this._protectionsPopup.toggleAttribute("hasException", hasException);

    this.iconBox.toggleAttribute("active", anyBlocking);
    this.iconBox.toggleAttribute("hasException", hasException);

    if (hasException) {
      this.iconBox.setAttribute(
        "tooltiptext",
        this.strings.disabledTooltipText
      );
      if (!this.hadShieldState && !isSimulated) {
        this.hadShieldState = true;
        this.shieldHistogramAdd(1);
      }
    } else if (anyBlocking) {
      this.iconBox.setAttribute("tooltiptext", this.strings.activeTooltipText);
      if (!this.hadShieldState && !isSimulated) {
        this.hadShieldState = true;
        this.shieldHistogramAdd(2);
      }
    } else {
      this.iconBox.removeAttribute("tooltiptext");
    }

    // We report up to one instance of fingerprinting and cryptomining
    // blocking and/or allowing per page load.
    let fingerprintingBlocking =
      Fingerprinting.isBlocking(event) &&
      !Fingerprinting.isBlocking(previousState);
    let fingerprintingAllowing =
      Fingerprinting.isAllowing(event) &&
      !Fingerprinting.isAllowing(previousState);
    let cryptominingBlocking =
      Cryptomining.isBlocking(event) && !Cryptomining.isBlocking(previousState);
    let cryptominingAllowing =
      Cryptomining.isAllowing(event) && !Cryptomining.isAllowing(previousState);

    if (fingerprintingBlocking) {
      this.fingerprintersHistogramAdd("blocked");
    } else if (fingerprintingAllowing) {
      this.fingerprintersHistogramAdd("allowed");
    }

    if (cryptominingBlocking) {
      this.cryptominersHistogramAdd("blocked");
    } else if (cryptominingAllowing) {
      this.cryptominersHistogramAdd("allowed");
    }
  },
  handleEvent(event) {
    let elem = document.activeElement;
    let position = elem.compareDocumentPosition(this._protectionsPopup);

    if (
      !(
        position &
        (Node.DOCUMENT_POSITION_CONTAINS | Node.DOCUMENT_POSITION_CONTAINED_BY)
      ) &&
      !this._protectionsPopup.hasAttribute("noautohide")
    ) {
      // Hide the panel when focusing an element that is
      // neither an ancestor nor descendant unless the panel has
      // @noautohide (e.g. for a tour).
      PanelMultiView.hidePopup(this._protectionsPopup);
    }
  },

  refreshProtectionsPopup() {
    let host = gIdentityHandler.getHostForDisplay();

    // Push the appropriate strings out to the UI.
    this._protectionsPopupMainViewHeaderLabel.textContent =
      // gNavigatorBundle.getFormattedString("protections.header", [host]);
      `Tracking Protections for ${host}`;

    let currentlyEnabled = !this._protectionsPopup.hasAttribute("hasException");

    for (let tpSwitch of [
      this._protectionsPopupTPSwitch,
      this._protectionsPopupSiteNotWorkingTPSwitch,
    ]) {
      tpSwitch.toggleAttribute("enabled", currentlyEnabled);
    }

    // Toggle the breakage link according to the current enable state.
    this.toggleBreakageLink();

    // Display a short TP switch section depending on the enable state. We need
    // to use a separate attribute here since the 'hasException' attribute will
    // be toggled as well as the TP switch, we cannot rely on that to decide the
    // height of TP switch section, or it will change when toggling the switch,
    // which is not desirable for us. So, we need to use a different attribute
    // here.
    this._protectionsPopupTPSwitchSection.toggleAttribute(
      "short",
      !currentlyEnabled
    );

    // Set the counter of the 'Trackers blocked This Week'.
    // We need to get the statistics of trackers. So far, we haven't implemented
    // this yet. So we use a fake number here. Should be resolved in
    // Bug 1555231.
    this.setTrackersBlockedCounter(244051);
  },

  disableForCurrentPage() {
    ContentBlockingAllowList.add(gBrowser.selectedBrowser);
    PanelMultiView.hidePopup(this._protectionsPopup);
    BrowserReload();
  },

  enableForCurrentPage() {
    ContentBlockingAllowList.remove(gBrowser.selectedBrowser);
    PanelMultiView.hidePopup(this._protectionsPopup);
    BrowserReload();
  },

  async onTPSwitchCommand(event) {
    // When the switch is clicked, we wait 500ms and then disable/enable
    // protections, causing the page to refresh, and close the popup.
    // We need to ensure we don't handle more clicks during the 500ms delay,
    // so we keep track of state and return early if needed.
    if (this._TPSwitchCommanding) {
      return;
    }

    this._TPSwitchCommanding = true;

    // Toggling the 'hasException' on the protections panel in order to do some
    // styling after toggling the TP switch.
    let newExceptionState = this._protectionsPopup.toggleAttribute(
      "hasException"
    );
    for (let tpSwitch of [
      this._protectionsPopupTPSwitch,
      this._protectionsPopupSiteNotWorkingTPSwitch,
    ]) {
      tpSwitch.toggleAttribute("enabled", !newExceptionState);
    }

    // Toggle the breakage link if needed.
    this.toggleBreakageLink();

    // Indicating that we need to show a toast after refreshing the page.
    // And caching the current URI and window ID in order to only show the mini
    // panel if it's still on the same page.
    this._showToastAfterRefresh = true;
    this._previousURI = gBrowser.currentURI.spec;
    this._previousOuterWindowID = gBrowser.selectedBrowser.outerWindowID;

    await new Promise(resolve => setTimeout(resolve, 500));

    if (newExceptionState) {
      this.disableForCurrentPage();
    } else {
      this.enableForCurrentPage();
    }

    delete this._TPSwitchCommanding;
  },

  setTrackersBlockedCounter(trackerCount) {
    this._protectionPopupTrackersCounterDescription.textContent =
      // gNavigatorBundle.getFormattedString(
      //   "protections.trackers_counter", [cnt]);
      `Trackers blocked this week: ${trackerCount.toLocaleString()}`;
  },

  /**
   * Showing the protections popup.
   *
   * @param {Object} options
   *                 The object could have two properties.
   *                 event:
   *                   The event triggers the protections popup to be opened.
   *                 toast:
   *                   A boolean to indicate if we need to open the protections
   *                   popup as a toast. A toast only has a header section and
   *                   will be hidden after a certain amount of time.
   */
  showProtectionsPopup(options = {}) {
    const { event, toast } = options;

    // We need to clear the toast timer if it exists before showing the
    // protections popup.
    if (this._toastPanelTimer) {
      clearTimeout(this._toastPanelTimer);
      delete this._toastPanelTimer;
    }

    // Make sure that the display:none style we set in xul is removed now that
    // the popup is actually needed
    this._protectionsPopup.hidden = false;

    this._protectionsPopup.toggleAttribute("toast", !!toast);
    if (!toast) {
      // Refresh strings if we want to open it as a standard protections popup.
      this.refreshProtectionsPopup();
    }

    if (toast) {
      this._protectionsPopup.addEventListener(
        "popupshown",
        () => {
          this._toastPanelTimer = setTimeout(() => {
            PanelMultiView.hidePopup(this._protectionsPopup);
            delete this._toastPanelTimer;
          }, this._protectionsPopupToastTimeout);
        },
        { once: true }
      );
    }

    // Check the panel state of the identity panel. Hide it if needed.
    if (gIdentityHandler._identityPopup.state != "closed") {
      PanelMultiView.hidePopup(gIdentityHandler._identityPopup);
    }

    // Now open the popup, anchored off the primary chrome element
    PanelMultiView.openPopup(
      this._protectionsPopup,
      gIdentityHandler._trackingProtectionIconContainer,
      {
        position: "bottomcenter topleft",
        triggerEvent: event,
      }
    ).catch(Cu.reportError);
  },

  showSiteNotWorkingView() {
    this._protectionsPopupMultiView.showSubView(
      "protections-popup-siteNotWorkingView"
    );
  },

  showSendReportView() {
    // Save this URI to make sure that the user really only submits the location
    // they see in the report breakage dialog.
    this.reportURI = gBrowser.currentURI;
    let urlWithoutQuery = this.reportURI.asciiSpec.replace(
      "?" + this.reportURI.query,
      ""
    );
    this._protectionsPopupSendReportURL.value = urlWithoutQuery;
    this._protectionsPopupMultiView.showSubView(
      "protections-popup-sendReportView"
    );
  },

  toggleBreakageLink() {
    // The breakage link will only be shown if tracking protection is enabled
    // for the site and the TP toggle state is on. And we won't show the
    // link as toggling TP switch to On from Off. In order to do so, we need to
    // know the previous TP state. We check the ContentBlockingAllowList instead
    // of 'hasException' attribute of the protection popup for the previous
    // since the 'hasException' will also be toggled as well as toggling the TP
    // switch. We won't be able to know the previous TP state through the
    // 'hasException' attribute. So we fallback to check the
    // ContentBlockingAllowList here.
    this._protectionsPopupTPSwitchBreakageLink.hidden =
      ContentBlockingAllowList.includes(gBrowser.selectedBrowser) ||
      !this._protectionsPopupTPSwitch.hasAttribute("enabled");
  },

  submitBreakageReport(uri) {
    let reportEndpoint = Services.prefs.getStringPref(
      this.PREF_REPORT_BREAKAGE_URL
    );
    if (!reportEndpoint) {
      return;
    }

    let commentsTextarea = document.getElementById(
      "protections-popup-sendReportView-collection-comments"
    );

    let formData = new FormData();
    formData.set("title", uri.host);

    // Leave the ? at the end of the URL to signify that this URL had its query stripped.
    let urlWithoutQuery = uri.asciiSpec.replace(uri.query, "");
    let body = `Full URL: ${urlWithoutQuery}\n`;
    body += `userAgent: ${navigator.userAgent}\n`;

    body += "\n**Preferences**\n";
    body += `${
      TrackingProtection.PREF_ENABLED_GLOBALLY
    }: ${Services.prefs.getBoolPref(
      TrackingProtection.PREF_ENABLED_GLOBALLY
    )}\n`;
    body += `${
      TrackingProtection.PREF_ENABLED_IN_PRIVATE_WINDOWS
    }: ${Services.prefs.getBoolPref(
      TrackingProtection.PREF_ENABLED_IN_PRIVATE_WINDOWS
    )}\n`;
    body += `urlclassifier.trackingTable: ${Services.prefs.getStringPref(
      "urlclassifier.trackingTable"
    )}\n`;
    body += `network.http.referer.defaultPolicy: ${Services.prefs.getIntPref(
      "network.http.referer.defaultPolicy"
    )}\n`;
    body += `network.http.referer.defaultPolicy.pbmode: ${Services.prefs.getIntPref(
      "network.http.referer.defaultPolicy.pbmode"
    )}\n`;
    body += `${ThirdPartyCookies.PREF_ENABLED}: ${Services.prefs.getIntPref(
      ThirdPartyCookies.PREF_ENABLED
    )}\n`;
    body += `network.cookie.lifetimePolicy: ${Services.prefs.getIntPref(
      "network.cookie.lifetimePolicy"
    )}\n`;
    body += `privacy.annotate_channels.strict_list.enabled: ${Services.prefs.getBoolPref(
      "privacy.annotate_channels.strict_list.enabled"
    )}\n`;
    body += `privacy.restrict3rdpartystorage.expiration: ${Services.prefs.getIntPref(
      "privacy.restrict3rdpartystorage.expiration"
    )}\n`;
    body += `${Fingerprinting.PREF_ENABLED}: ${Services.prefs.getBoolPref(
      Fingerprinting.PREF_ENABLED
    )}\n`;
    body += `${Cryptomining.PREF_ENABLED}: ${Services.prefs.getBoolPref(
      Cryptomining.PREF_ENABLED
    )}\n`;

    body += "\n**Comments**\n" + commentsTextarea.value;

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
    })
      .then(function(response) {
        if (!response.ok) {
          Cu.reportError(
            `Content Blocking report to ${reportEndpoint} failed with status ${
              response.status
            }`
          );
        } else {
          // Clear the textarea value when the report is submitted
          commentsTextarea.value = "";
        }
      })
      .catch(Cu.reportError);
  },

  onSendReportClicked() {
    this._protectionsPopup.hidePopup();
    this.submitBreakageReport(this.reportURI);
  },
};
