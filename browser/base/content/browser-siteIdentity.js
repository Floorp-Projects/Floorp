/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

/**
 * Utility object to handle manipulations of the identity indicators in the UI
 */
var gIdentityHandler = {
  /**
   * nsIURI for which the identity UI is displayed. This has been already
   * processed by nsIURIFixup.createExposableURI.
   */
  _uri: null,

  /**
   * We only know the connection type if this._uri has a defined "host" part.
   *
   * These URIs, like "about:", "file:" and "data:" URIs, will usually be treated as a
   * an unknown connection.
   */
  _uriHasHost: false,

  /**
   * If this tab belongs to a WebExtension, contains its WebExtensionPolicy.
   */
  _pageExtensionPolicy: null,

  /**
   * Whether this._uri refers to an internally implemented browser page.
   *
   * Note that this is set for some "about:" pages, but general "chrome:" URIs
   * are not included in this category by default.
   */
  _isSecureInternalUI: false,

  /**
   * Whether the content window is considered a "secure context". This
   * includes "potentially trustworthy" origins such as file:// URLs or localhost.
   * https://w3c.github.io/webappsec-secure-contexts/#is-origin-trustworthy
   */
  _isSecureContext: false,

  /**
   * nsITransportSecurityInfo metadata provided by gBrowser.securityUI the last
   * time the identity UI was updated, or null if the connection is not secure.
   */
  _secInfo: null,

  /**
   * Bitmask provided by nsIWebProgressListener.onSecurityChange.
   */
  _state: 0,

  /**
   * RegExp used to decide if an about url should be shown as being part of
   * the browser UI.
   */
  _secureInternalUIWhitelist: /^(?:accounts|addons|cache|certificate|config|crashes|downloads|license|logins|preferences|protections|rights|sessionrestore|support|welcomeback)(?:[?#]|$)/i,

  /**
   * Whether the established HTTPS connection is considered "broken".
   * This could have several reasons, such as mixed content or weak
   * cryptography. If this is true, _isSecureConnection is false.
   */
  get _isBrokenConnection() {
    return this._state & Ci.nsIWebProgressListener.STATE_IS_BROKEN;
  },

  /**
   * Whether the connection to the current site was done via secure
   * transport. Note that this attribute is not true in all cases that
   * the site was accessed via HTTPS, i.e. _isSecureConnection will
   * be false when _isBrokenConnection is true, even though the page
   * was loaded over HTTPS.
   */
  get _isSecureConnection() {
    // If a <browser> is included within a chrome document, then this._state
    // will refer to the security state for the <browser> and not the top level
    // document. In this case, don't upgrade the security state in the UI
    // with the secure state of the embedded <browser>.
    return (
      !this._isURILoadedFromFile &&
      this._state & Ci.nsIWebProgressListener.STATE_IS_SECURE
    );
  },

  get _isEV() {
    // If a <browser> is included within a chrome document, then this._state
    // will refer to the security state for the <browser> and not the top level
    // document. In this case, don't upgrade the security state in the UI
    // with the EV state of the embedded <browser>.
    return (
      !this._isURILoadedFromFile &&
      this._state & Ci.nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL
    );
  },

  get _isMixedActiveContentLoaded() {
    return (
      this._state & Ci.nsIWebProgressListener.STATE_LOADED_MIXED_ACTIVE_CONTENT
    );
  },

  get _isMixedActiveContentBlocked() {
    return (
      this._state & Ci.nsIWebProgressListener.STATE_BLOCKED_MIXED_ACTIVE_CONTENT
    );
  },

  get _isMixedPassiveContentLoaded() {
    return (
      this._state & Ci.nsIWebProgressListener.STATE_LOADED_MIXED_DISPLAY_CONTENT
    );
  },

  get _isCertUserOverridden() {
    return this._state & Ci.nsIWebProgressListener.STATE_CERT_USER_OVERRIDDEN;
  },

  get _isCertDistrustImminent() {
    return this._state & Ci.nsIWebProgressListener.STATE_CERT_DISTRUST_IMMINENT;
  },

  get _hasInsecureLoginForms() {
    // checks if the page has been flagged for an insecure login. Also checks
    // if the pref to degrade the UI is set to true
    return (
      LoginManagerParent.hasInsecureLoginForms(gBrowser.selectedBrowser) &&
      Services.prefs.getBoolPref("security.insecure_password.ui.enabled")
    );
  },

  // smart getters
  get _identityPopup() {
    delete this._identityPopup;
    return (this._identityPopup = document.getElementById("identity-popup"));
  },
  get _identityBox() {
    delete this._identityBox;
    return (this._identityBox = document.getElementById("identity-box"));
  },
  get _identityPopupMultiView() {
    delete this._identityPopupMultiView;
    return (this._identityPopupMultiView = document.getElementById(
      "identity-popup-multiView"
    ));
  },
  get _identityPopupMainView() {
    delete this._identityPopupMainView;
    return (this._identityPopupMainView = document.getElementById(
      "identity-popup-mainView"
    ));
  },
  get _identityPopupMainViewHeaderLabel() {
    delete this._identityPopupMainViewHeaderLabel;
    return (this._identityPopupMainViewHeaderLabel = document.getElementById(
      "identity-popup-mainView-panel-header-span"
    ));
  },
  get _identityPopupSecurityEVContentOwner() {
    delete this._identityPopupSecurityEVContentOwner;
    return (this._identityPopupSecurityEVContentOwner = document.getElementById(
      "identity-popup-security-ev-content-owner"
    ));
  },
  get _identityPopupContentOwner() {
    delete this._identityPopupContentOwner;
    return (this._identityPopupContentOwner = document.getElementById(
      "identity-popup-content-owner"
    ));
  },
  get _identityPopupContentSupp() {
    delete this._identityPopupContentSupp;
    return (this._identityPopupContentSupp = document.getElementById(
      "identity-popup-content-supplemental"
    ));
  },
  get _identityPopupContentVerif() {
    delete this._identityPopupContentVerif;
    return (this._identityPopupContentVerif = document.getElementById(
      "identity-popup-content-verifier"
    ));
  },
  get _identityPopupCustomRootLearnMore() {
    delete this._identityPopupCustomRootLearnMore;
    return (this._identityPopupCustomRootLearnMore = document.getElementById(
      "identity-popup-custom-root-learn-more"
    ));
  },
  get _identityPopupMixedContentLearnMore() {
    delete this._identityPopupMixedContentLearnMore;
    return (this._identityPopupMixedContentLearnMore = [
      ...document.querySelectorAll(".identity-popup-mcb-learn-more"),
    ]);
  },
  get _identityPopupInsecureLoginFormsLearnMore() {
    delete this._identityPopupInsecureLoginFormsLearnMore;
    return (this._identityPopupInsecureLoginFormsLearnMore = document.getElementById(
      "identity-popup-insecure-login-forms-learn-more"
    ));
  },
  get _identityIconLabels() {
    delete this._identityIconLabels;
    return (this._identityIconLabels = document.getElementById(
      "identity-icon-labels"
    ));
  },
  get _identityIconLabel() {
    delete this._identityIconLabel;
    return (this._identityIconLabel = document.getElementById(
      "identity-icon-label"
    ));
  },
  get _overrideService() {
    delete this._overrideService;
    return (this._overrideService = Cc[
      "@mozilla.org/security/certoverride;1"
    ].getService(Ci.nsICertOverrideService));
  },
  get _identityIconCountryLabel() {
    delete this._identityIconCountryLabel;
    return (this._identityIconCountryLabel = document.getElementById(
      "identity-icon-country-label"
    ));
  },
  get _identityIcon() {
    delete this._identityIcon;
    return (this._identityIcon = document.getElementById("identity-icon"));
  },
  get _permissionList() {
    delete this._permissionList;
    return (this._permissionList = document.getElementById(
      "identity-popup-permission-list"
    ));
  },
  get _permissionEmptyHint() {
    delete this._permissionEmptyHint;
    return (this._permissionEmptyHint = document.getElementById(
      "identity-popup-permission-empty-hint"
    ));
  },
  get _permissionReloadHint() {
    delete this._permissionReloadHint;
    return (this._permissionReloadHint = document.getElementById(
      "identity-popup-permission-reload-hint"
    ));
  },
  get _popupExpander() {
    delete this._popupExpander;
    return (this._popupExpander = document.getElementById(
      "identity-popup-security-expander"
    ));
  },
  get _clearSiteDataFooter() {
    delete this._clearSiteDataFooter;
    return (this._clearSiteDataFooter = document.getElementById(
      "identity-popup-clear-sitedata-footer"
    ));
  },
  get _permissionAnchors() {
    delete this._permissionAnchors;
    let permissionAnchors = {};
    for (let anchor of document.getElementById("blocked-permissions-container")
      .children) {
      permissionAnchors[anchor.getAttribute("data-permission-id")] = anchor;
    }
    return (this._permissionAnchors = permissionAnchors);
  },
  get _trackingProtectionIconContainer() {
    delete this._trackingProtectionIconContainer;
    return (this._trackingProtectionIconContainer = document.getElementById(
      "tracking-protection-icon-container"
    ));
  },

  get _geoSharingIcon() {
    delete this._geoSharingIcon;
    return (this._geoSharingIcon = document.getElementById("geo-sharing-icon"));
  },

  get _webRTCSharingIcon() {
    delete this._webRTCSharingIcon;
    return (this._webRTCSharingIcon = document.getElementById(
      "webrtc-sharing-icon"
    ));
  },

  get _insecureConnectionIconEnabled() {
    delete this._insecureConnectionIconEnabled;
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_insecureConnectionIconEnabled",
      "security.insecure_connection_icon.enabled"
    );
    return this._insecureConnectionIconEnabled;
  },
  get _insecureConnectionIconPBModeEnabled() {
    delete this._insecureConnectionIconPBModeEnabled;
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_insecureConnectionIconPBModeEnabled",
      "security.insecure_connection_icon.pbmode.enabled"
    );
    return this._insecureConnectionIconPBModeEnabled;
  },
  get _insecureConnectionTextEnabled() {
    delete this._insecureConnectionTextEnabled;
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_insecureConnectionTextEnabled",
      "security.insecure_connection_text.enabled"
    );
    return this._insecureConnectionTextEnabled;
  },
  get _insecureConnectionTextPBModeEnabled() {
    delete this._insecureConnectionTextPBModeEnabled;
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_insecureConnectionTextPBModeEnabled",
      "security.insecure_connection_text.pbmode.enabled"
    );
    return this._insecureConnectionTextPBModeEnabled;
  },
  get _protectionsPanelEnabled() {
    delete this._protectionsPanelEnabled;
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_protectionsPanelEnabled",
      "browser.protections_panel.enabled",
      false
    );
    return this._protectionsPanelEnabled;
  },

  get _useGrayLockIcon() {
    delete this._useGrayLockIcon;
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_useGrayLockIcon",
      "security.secure_connection_icon_color_gray",
      false
    );
    return this._useGrayLockIcon;
  },

  /**
   * Handles clicks on the "Clear Cookies and Site Data" button.
   */
  async clearSiteData(event) {
    if (!this._uriHasHost) {
      return;
    }

    let host = this._uri.host;

    // Site data could have changed while the identity popup was open,
    // reload again to be sure.
    await SiteDataManager.updateSites();

    let baseDomain = SiteDataManager.getBaseDomainFromHost(host);
    let siteData = await SiteDataManager.getSites(baseDomain);

    // Hide the popup before showing the removal prompt, to
    // avoid a pretty ugly transition. Also hide it even
    // if the update resulted in no site data, to keep the
    // illusion that clicking the button had an effect.
    PanelMultiView.hidePopup(this._identityPopup);

    if (siteData && siteData.length) {
      let hosts = siteData.map(site => site.host);
      if (SiteDataManager.promptSiteDataRemoval(window, hosts)) {
        SiteDataManager.remove(hosts);
      }
    }

    event.stopPropagation();
  },

  openPermissionPreferences() {
    openPreferences("privacy-permissions");
  },

  recordClick(object) {
    Services.telemetry.recordEvent(
      "security.ui.identitypopup",
      "click",
      object
    );
  },

  /**
   * Handler for mouseclicks on the "More Information" button in the
   * "identity-popup" panel.
   */
  handleMoreInfoClick(event) {
    displaySecurityInfo();
    event.stopPropagation();
    PanelMultiView.hidePopup(this._identityPopup);
  },

  showSecuritySubView() {
    this._identityPopupMultiView.showSubView(
      "identity-popup-securityView",
      this._popupExpander
    );

    // Elements of hidden views have -moz-user-focus:ignore but setting that
    // per CSS selector doesn't blur a focused element in those hidden views.
    Services.focus.clearFocus(window);
  },

  disableMixedContentProtection() {
    // Use telemetry to measure how often unblocking happens
    const kMIXED_CONTENT_UNBLOCK_EVENT = 2;
    let histogram = Services.telemetry.getHistogramById(
      "MIXED_CONTENT_UNBLOCK_COUNTER"
    );
    histogram.add(kMIXED_CONTENT_UNBLOCK_EVENT);
    // Reload the page with the content unblocked
    BrowserReloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_MIXED_CONTENT);
    PanelMultiView.hidePopup(this._identityPopup);
  },

  enableMixedContentProtection() {
    gBrowser.selectedBrowser.sendMessageToActor(
      "MixedContent:ReenableProtection",
      {},
      "BrowserTab"
    );
    BrowserReload();
    PanelMultiView.hidePopup(this._identityPopup);
  },

  removeCertException() {
    if (!this._uriHasHost) {
      Cu.reportError(
        "Trying to revoke a cert exception on a URI without a host?"
      );
      return;
    }
    let host = this._uri.host;
    let port = this._uri.port > 0 ? this._uri.port : 443;
    this._overrideService.clearValidityOverride(host, port);
    BrowserReloadSkipCache();
    PanelMultiView.hidePopup(this._identityPopup);
  },

  /**
   * Helper to parse out the important parts of _secInfo (of the SSL cert in
   * particular) for use in constructing identity UI strings
   */
  getIdentityData() {
    var result = {};
    var cert = this._secInfo.serverCert;

    // Human readable name of Subject
    result.subjectOrg = cert.organization;

    // SubjectName fields, broken up for individual access
    if (cert.subjectName) {
      result.subjectNameFields = {};
      cert.subjectName.split(",").forEach(function(v) {
        var field = v.split("=");
        this[field[0]] = field[1];
      }, result.subjectNameFields);

      // Call out city, state, and country specifically
      result.city = result.subjectNameFields.L;
      result.state = result.subjectNameFields.ST;
      result.country = result.subjectNameFields.C;
    }

    // Human readable name of Certificate Authority
    result.caOrg = cert.issuerOrganization || cert.issuerCommonName;
    result.cert = cert;

    return result;
  },

  /**
   * Update the identity user interface for the page currently being displayed.
   *
   * This examines the SSL certificate metadata, if available, as well as the
   * connection type and other security-related state information for the page.
   *
   * @param state
   *        Bitmask provided by nsIWebProgressListener.onSecurityChange.
   * @param uri
   *        nsIURI for which the identity UI should be displayed, already
   *        processed by nsIURIFixup.createExposableURI.
   */
  updateIdentity(state, uri) {
    let shouldHidePopup = this._uri && this._uri.spec != uri.spec;
    this._state = state;

    // Firstly, populate the state properties required to display the UI. See
    // the documentation of the individual properties for details.
    this.setURI(uri);
    this._secInfo = gBrowser.securityUI.secInfo;
    this._isSecureContext = gBrowser.securityUI.isSecureContext;

    // Then, update the user interface with the available data.
    this.refreshIdentityBlock();
    // Handle a location change while the Control Center is focused
    // by closing the popup (bug 1207542)
    if (shouldHidePopup) {
      PanelMultiView.hidePopup(this._identityPopup);
    }

    // NOTE: We do NOT update the identity popup (the control center) when
    // we receive a new security state on the existing page (i.e. from a
    // subframe). If the user opened the popup and looks at the provided
    // information we don't want to suddenly change the panel contents.

    // Finally, if there are warnings to issue, issue them
    if (this._isCertDistrustImminent) {
      let consoleMsg = Cc["@mozilla.org/scripterror;1"].createInstance(
        Ci.nsIScriptError
      );
      let windowId = gBrowser.selectedBrowser.innerWindowID;
      let message = gBrowserBundle.GetStringFromName(
        "certImminentDistrust.message"
      );
      // Use uri.prePath instead of initWithSourceURI() so that these can be
      // de-duplicated on the scheme+host+port combination.
      consoleMsg.initWithWindowID(
        message,
        uri.prePath,
        null,
        0,
        0,
        Ci.nsIScriptError.warningFlag,
        "SSL",
        windowId
      );
      Services.console.logMessage(consoleMsg);
    }
  },

  /**
   * This is called asynchronously when requested by the Logins module, after
   * the insecure login forms state for the page has been updated.
   */
  refreshForInsecureLoginForms() {
    // Check this._uri because we don't want to refresh the user interface if
    // this is called before the first page load in the window for any reason.
    if (!this._uri) {
      return;
    }
    this.refreshIdentityBlock();
  },

  updateSharingIndicator() {
    let tab = gBrowser.selectedTab;
    this._sharingState = tab._sharingState;

    this._webRTCSharingIcon.removeAttribute("paused");
    this._webRTCSharingIcon.removeAttribute("sharing");
    this._geoSharingIcon.removeAttribute("sharing");

    if (this._sharingState) {
      if (
        this._sharingState &&
        this._sharingState.webRTC &&
        this._sharingState.webRTC.sharing
      ) {
        this._webRTCSharingIcon.setAttribute(
          "sharing",
          this._sharingState.webRTC.sharing
        );

        if (this._sharingState.webRTC.paused) {
          this._webRTCSharingIcon.setAttribute("paused", "true");
        }
      }
      if (this._sharingState.geo) {
        this._geoSharingIcon.setAttribute("sharing", this._sharingState.geo);
      }
    }

    if (this._identityPopup.state == "open") {
      this.updateSitePermissions();
      PanelView.forNode(
        this._identityPopupMainView
      ).descriptionHeightWorkaround();
    }
  },

  /**
   * Attempt to provide proper IDN treatment for host names
   */
  getEffectiveHost() {
    if (!this._IDNService) {
      this._IDNService = Cc["@mozilla.org/network/idn-service;1"].getService(
        Ci.nsIIDNService
      );
    }
    try {
      return this._IDNService.convertToDisplayIDN(this._uri.host, {});
    } catch (e) {
      // If something goes wrong (e.g. host is an IP address) just fail back
      // to the full domain.
      return this._uri.host;
    }
  },

  getHostForDisplay() {
    let host = "";

    try {
      host = this.getEffectiveHost();
    } catch (e) {
      // Some URIs might have no hosts.
    }

    let readerStrippedURI = ReaderMode.getOriginalUrlObjectForDisplay(
      this._uri.displaySpec
    );
    if (readerStrippedURI) {
      host = readerStrippedURI.host;
    }

    if (this._pageExtensionPolicy) {
      host = this._pageExtensionPolicy.name;
    }

    // Fallback for special protocols.
    if (!host) {
      host = this._uri.specIgnoringRef;
    }

    return host;
  },

  /**
   * Return the CSS class name to set on the "fullscreen-warning" element to
   * display information about connection security in the notification shown
   * when a site enters the fullscreen mode.
   */
  get pointerlockFsWarningClassName() {
    // Note that the fullscreen warning does not handle _isSecureInternalUI.
    if (this._uriHasHost && this._isEV) {
      return "verifiedIdentity";
    }
    if (this._uriHasHost && this._isSecureConnection) {
      return "verifiedDomain";
    }
    return "unknownIdentity";
  },

  /**
   * Returns whether the issuer of the current certificate chain is
   * built-in (returns false) or imported (returns true).
   */
  _hasCustomRoot() {
    let issuerCert = null;
    // Walk the whole chain to get the last cert.
    // eslint-disable-next-line no-empty
    for (issuerCert of this._secInfo.succeededCertChain.getEnumerator()) {
    }

    return !issuerCert.isBuiltInRoot;
  },

  /**
   * Returns whether the current URI results in an "invalid"
   * URL bar state, which effectively means hidden security
   * indicators.
   */
  _hasInvalidPageProxyState() {
    return (
      !this._uriHasHost &&
      this._uri &&
      isBlankPageURL(this._uri.spec) &&
      !this._uri.schemeIs("moz-extension")
    );
  },

  /**
   * Updates the identity block user interface with the data from this object.
   */
  refreshIdentityBlock() {
    if (!this._identityBox) {
      return;
    }

    // If this condition is true, the URL bar will have an "invalid"
    // pageproxystate, which will hide the security indicators. Thus, we can
    // safely avoid updating the security UI.
    //
    // This will also filter out intermediate about:blank loads to avoid
    // flickering the identity block and doing unnecessary work.
    if (this._hasInvalidPageProxyState()) {
      return;
    }

    let icon_label = "";
    let tooltip = "";
    let icon_country_label = "";
    let icon_labels_dir = "ltr";

    if (this._isSecureInternalUI) {
      // This is a secure internal Firefox page.
      this._identityBox.className = "chromeUI";
      let brandBundle = document.getElementById("bundle_brand");
      icon_label = brandBundle.getString("brandShorterName");
    } else if (this._uriHasHost && this._isEV) {
      // This is a secure connection with EV.
      this._identityBox.className = "verifiedIdentity";
      if (this._isMixedActiveContentBlocked) {
        this._identityBox.classList.add("mixedActiveBlocked");
      }

      if (!this._isCertUserOverridden) {
        // If it's identified, then we can populate the dialog with credentials
        let iData = this.getIdentityData();
        tooltip = gNavigatorBundle.getFormattedString(
          "identity.identified.verifier",
          [iData.caOrg]
        );
        icon_label = iData.subjectOrg;
        if (iData.country) {
          icon_country_label = "(" + iData.country + ")";
        }

        // If the organization name starts with an RTL character, then
        // swap the positions of the organization and country code labels.
        // The Unicode ranges reflect the definition of the UTF16_CODE_UNIT_IS_BIDI
        // macro in intl/unicharutil/util/nsBidiUtils.h. When bug 218823 gets
        // fixed, this test should be replaced by one adhering to the
        // Unicode Bidirectional Algorithm proper (at the paragraph level).
        icon_labels_dir = /^[\u0590-\u08ff\ufb1d-\ufdff\ufe70-\ufefc\ud802\ud803\ud83a\ud83b]/.test(
          icon_label
        )
          ? "rtl"
          : "ltr";
      }
    } else if (this._pageExtensionPolicy) {
      // This is a WebExtension page.
      this._identityBox.className = "extensionPage";
      let extensionName = this._pageExtensionPolicy.name;
      icon_label = gNavigatorBundle.getFormattedString(
        "identity.extension.label",
        [extensionName]
      );
    } else if (this._uriHasHost && this._isSecureConnection) {
      // This is a secure connection.
      this._identityBox.className = "verifiedDomain";
      if (this._isMixedActiveContentBlocked) {
        this._identityBox.classList.add("mixedActiveBlocked");
      }
      if (!this._isCertUserOverridden) {
        // It's a normal cert, verifier is the CA Org.
        tooltip = gNavigatorBundle.getFormattedString(
          "identity.identified.verifier",
          [this.getIdentityData().caOrg]
        );
      }
    } else if (this._isBrokenConnection) {
      // This is a secure connection, but something is wrong.
      this._identityBox.className = "unknownIdentity";

      if (this._isMixedActiveContentLoaded) {
        this._identityBox.classList.add("mixedActiveContent");
      } else if (this._isMixedActiveContentBlocked) {
        this._identityBox.classList.add(
          "mixedDisplayContentLoadedActiveBlocked"
        );
      } else if (this._isMixedPassiveContentLoaded) {
        this._identityBox.classList.add("mixedDisplayContent");
      } else {
        this._identityBox.classList.add("weakCipher");
      }
    } else if (
      this._isSecureContext ||
      (gBrowser.selectedBrowser.documentURI &&
        (gBrowser.selectedBrowser.documentURI.scheme == "about" ||
          gBrowser.selectedBrowser.documentURI.scheme == "chrome"))
    ) {
      // This is a local resource (and shouldn't be marked insecure).
      this._identityBox.className = "unknownIdentity";
    } else {
      // This is an insecure connection.
      let warnOnInsecure =
        this._insecureConnectionIconEnabled ||
        (this._insecureConnectionIconPBModeEnabled &&
          PrivateBrowsingUtils.isWindowPrivate(window));
      let className = warnOnInsecure ? "notSecure" : "unknownIdentity";
      this._identityBox.className = className;
      tooltip = warnOnInsecure
        ? gNavigatorBundle.getString("identity.notSecure.tooltip")
        : "";

      let warnTextOnInsecure =
        this._insecureConnectionTextEnabled ||
        (this._insecureConnectionTextPBModeEnabled &&
          PrivateBrowsingUtils.isWindowPrivate(window));
      if (warnTextOnInsecure) {
        icon_label = gNavigatorBundle.getString("identity.notSecure.label");
        this._identityBox.classList.add("notSecureText");
      }
      if (this._hasInsecureLoginForms) {
        // Insecure login forms can only be present on "unknown identity"
        // pages, either already insecure or with mixed active content loaded.
        this._identityBox.classList.add("insecureLoginForms");
      }
    }

    // Hide the shield icon if it is a chrome page.
    this._trackingProtectionIconContainer.classList.toggle(
      "chromeUI",
      this._isSecureInternalUI
    );

    if (this._isCertUserOverridden) {
      this._identityBox.classList.add("certUserOverridden");
      // Cert is trusted because of a security exception, verifier is a special string.
      tooltip = gNavigatorBundle.getString(
        "identity.identified.verified_by_you"
      );
    }

    let permissionAnchors = this._permissionAnchors;

    // hide all permission icons
    for (let icon of Object.values(permissionAnchors)) {
      icon.removeAttribute("showing");
    }

    // keeps track if we should show an indicator that there are active permissions
    let hasGrantedPermissions = false;

    // show permission icons
    let permissions = SitePermissions.getAllForBrowser(
      gBrowser.selectedBrowser
    );
    for (let permission of permissions) {
      if (
        permission.state == SitePermissions.BLOCK ||
        permission.state == SitePermissions.AUTOPLAY_BLOCKED_ALL
      ) {
        let icon = permissionAnchors[permission.id];
        if (icon) {
          icon.setAttribute("showing", "true");
        }
      } else if (permission.state != SitePermissions.UNKNOWN) {
        hasGrantedPermissions = true;
      }
    }

    if (hasGrantedPermissions) {
      this._identityBox.classList.add("grantedPermissions");
    }

    // Show blocked popup icon in the identity-box if popups are blocked
    // irrespective of popup permission capability value.
    if (
      gBrowser.selectedBrowser.blockedPopups &&
      gBrowser.selectedBrowser.blockedPopups.length
    ) {
      let icon = permissionAnchors.popup;
      icon.setAttribute("showing", "true");
    }

    // Gray lock icon for secure connections if pref set
    this._updateAttribute(
      this._identityIcon,
      "lock-icon-gray",
      this._useGrayLockIcon
    );

    // Push the appropriate strings out to the UI
    this._identityIcon.setAttribute("tooltiptext", tooltip);

    if (this._pageExtensionPolicy) {
      let extensionName = this._pageExtensionPolicy.name;
      this._identityIcon.setAttribute(
        "tooltiptext",
        gNavigatorBundle.getFormattedString("identity.extension.tooltip", [
          extensionName,
        ])
      );
    }

    this._identityIconLabels.setAttribute("tooltiptext", tooltip);
    this._identityIconLabel.setAttribute("value", icon_label);
    this._identityIconCountryLabel.setAttribute("value", icon_country_label);
    // Set cropping and direction
    this._identityIconLabel.setAttribute(
      "crop",
      icon_country_label ? "end" : "center"
    );
    this._identityIconLabel.parentNode.style.direction = icon_labels_dir;
    // Hide completely if the organization label is empty
    this._identityIconLabel.parentNode.collapsed = !icon_label;
  },

  /**
   * Set up the title and content messages for the identity message popup,
   * based on the specified mode, and the details of the SSL cert, where
   * applicable
   */
  refreshIdentityPopup() {
    // Update cookies and site data information and show the
    // "Clear Site Data" button if the site is storing local data.
    this._clearSiteDataFooter.hidden = true;
    if (this._uriHasHost) {
      let host = this._uri.host;
      SiteDataManager.updateSites().then(async () => {
        let baseDomain = SiteDataManager.getBaseDomainFromHost(host);
        let siteData = await SiteDataManager.getSites(baseDomain);

        if (siteData && siteData.length) {
          this._clearSiteDataFooter.hidden = false;
        }
      });
    }

    // Update "Learn More" for Mixed Content Blocking and Insecure Login Forms.
    let baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");
    this._identityPopupMixedContentLearnMore.forEach(e =>
      e.setAttribute("href", baseURL + "mixed-content")
    );
    this._identityPopupInsecureLoginFormsLearnMore.setAttribute(
      "href",
      baseURL + "insecure-password"
    );
    this._identityPopupCustomRootLearnMore.setAttribute(
      "href",
      baseURL + "enterprise-roots"
    );

    // This is in the properties file because the expander used to switch its tooltip.
    this._popupExpander.tooltipText = gNavigatorBundle.getString(
      "identity.showDetails.tooltip"
    );

    let customRoot = false;

    // Determine connection security information.
    let connection = "not-secure";
    if (this._isSecureInternalUI) {
      connection = "chrome";
    } else if (this._pageExtensionPolicy) {
      connection = "extension";
    } else if (this._isURILoadedFromFile) {
      connection = "file";
    } else if (this._isEV) {
      connection = "secure-ev";
    } else if (this._isCertUserOverridden) {
      connection = "secure-cert-user-overridden";
    } else if (this._isSecureConnection) {
      connection = "secure";
      customRoot = this._hasCustomRoot();
    }

    // Determine if there are insecure login forms.
    let loginforms = "secure";
    if (this._hasInsecureLoginForms) {
      loginforms = "insecure";
    }

    // Determine the mixed content state.
    let mixedcontent = [];
    if (this._isMixedPassiveContentLoaded) {
      mixedcontent.push("passive-loaded");
    }
    if (this._isMixedActiveContentLoaded) {
      mixedcontent.push("active-loaded");
    } else if (this._isMixedActiveContentBlocked) {
      mixedcontent.push("active-blocked");
    }
    mixedcontent = mixedcontent.join(" ");

    // We have no specific flags for weak ciphers (yet). If a connection is
    // broken and we can't detect any mixed content loaded then it's a weak
    // cipher.
    let ciphers = "";
    if (
      this._isBrokenConnection &&
      !this._isMixedActiveContentLoaded &&
      !this._isMixedPassiveContentLoaded
    ) {
      ciphers = "weak";
    }

    // Gray lock icon for secure connections if pref set
    this._updateAttribute(
      this._identityPopup,
      "lock-icon-gray",
      this._useGrayLockIcon
    );

    // Update all elements.
    let elementIDs = ["identity-popup", "identity-popup-securityView-body"];

    for (let id of elementIDs) {
      let element = document.getElementById(id);
      this._updateAttribute(element, "connection", connection);
      this._updateAttribute(element, "loginforms", loginforms);
      this._updateAttribute(element, "ciphers", ciphers);
      this._updateAttribute(element, "mixedcontent", mixedcontent);
      this._updateAttribute(element, "isbroken", this._isBrokenConnection);
      this._updateAttribute(element, "customroot", customRoot);
    }

    // Initialize the optional strings to empty values
    let supplemental = "";
    let verifier = "";
    let host = this.getHostForDisplay();
    let owner = "";

    // Fill in the CA name if we have a valid TLS certificate.
    if (this._isSecureConnection || this._isCertUserOverridden) {
      verifier = this._identityIconLabels.tooltipText;
    }

    // Fill in organization information if we have a valid EV certificate.
    if (this._isEV) {
      let iData = this.getIdentityData();
      owner = iData.subjectOrg;
      verifier = this._identityIconLabels.tooltipText;

      // Build an appropriate supplemental block out of whatever location data we have
      if (iData.city) {
        supplemental += iData.city + "\n";
      }
      if (iData.state && iData.country) {
        supplemental += gNavigatorBundle.getFormattedString(
          "identity.identified.state_and_country",
          [iData.state, iData.country]
        );
      } else if (iData.state) {
        // State only
        supplemental += iData.state;
      } else if (iData.country) {
        // Country only
        supplemental += iData.country;
      }
    }

    // Push the appropriate strings out to the UI.
    this._identityPopupMainViewHeaderLabel.textContent = gNavigatorBundle.getFormattedString(
      "identity.headerMainWithHost",
      [host]
    );

    this._identityPopupSecurityEVContentOwner.textContent = gNavigatorBundle.getFormattedString(
      "identity.ev.contentOwner",
      [owner]
    );

    this._identityPopupContentOwner.textContent = owner;
    this._identityPopupContentSupp.textContent = supplemental;
    this._identityPopupContentVerif.textContent = verifier;

    // Update per-site permissions section.
    this.updateSitePermissions();
  },

  setURI(uri) {
    this._uri = uri;

    try {
      // Account for file: urls and catch when "" is the value
      this._uriHasHost = !!this._uri.host;
    } catch (ex) {
      this._uriHasHost = false;
    }

    this._isSecureInternalUI =
      uri.schemeIs("about") &&
      this._secureInternalUIWhitelist.test(uri.pathQueryRef);

    this._pageExtensionPolicy = WebExtensionPolicy.getByURI(uri);

    // Create a channel for the sole purpose of getting the resolved URI
    // of the request to determine if it's loaded from the file system.
    this._isURILoadedFromFile = false;
    let chanOptions = { uri: this._uri, loadUsingSystemPrincipal: true };
    let resolvedURI;
    try {
      resolvedURI = NetUtil.newChannel(chanOptions).URI;
      if (resolvedURI.schemeIs("jar")) {
        // Given a URI "jar:<jar-file-uri>!/<jar-entry>"
        // create a new URI using <jar-file-uri>!/<jar-entry>
        resolvedURI = NetUtil.newURI(resolvedURI.pathQueryRef);
      }
      // Check the URI again after resolving.
      this._isURILoadedFromFile = resolvedURI.schemeIs("file");
    } catch (ex) {
      // NetUtil's methods will throw for malformed URIs and the like
    }
  },

  /**
   * Click handler for the identity-box element in primary chrome.
   */
  handleIdentityButtonEvent(event) {
    event.stopPropagation();

    if (
      (event.type == "click" && event.button != 0) ||
      (event.type == "keypress" &&
        event.charCode != KeyEvent.DOM_VK_SPACE &&
        event.keyCode != KeyEvent.DOM_VK_RETURN)
    ) {
      return; // Left click, space or enter only
    }

    // Don't allow left click, space or enter if the location has been modified,
    // so long as we're not sharing any devices.
    // If we are sharing a device, the identity block is prevented by CSS from
    // being focused (and therefore, interacted with) by the user. However, we
    // want to allow opening the identity popup from the device control menu,
    // which calls click() on the identity button, so we don't return early.
    if (
      !this._sharingState &&
      gURLBar.getAttribute("pageproxystate") != "valid"
    ) {
      return;
    }

    // If we are in DOM full-screen, exit it before showing the identity popup
    if (document.fullscreen) {
      // Open the identity popup after DOM full-screen exit
      // We need to wait for the exit event and after that wait for the fullscreen exit transition to complete
      // If we call _openPopup before the full-screen transition ends it can get cancelled
      // Only waiting for painted is not sufficient because we could still be in the full-screen enter transition.
      let exitedEventReceived = false;
      window.messageManager.addMessageListener(
        "DOMFullscreen:Painted",
        function listener() {
          if (!exitedEventReceived) {
            return;
          }
          window.messageManager.removeMessageListener(
            "DOMFullscreen:Painted",
            listener
          );
          gIdentityHandler._openPopup(event);
        }
      );
      window.addEventListener(
        "MozDOMFullscreen:Exited",
        () => {
          exitedEventReceived = true;
        },
        { once: true }
      );
      document.exitFullscreen();
      return;
    }
    this._openPopup(event);
  },

  _openPopup(event) {
    // Make sure that the display:none style we set in xul is removed now that
    // the popup is actually needed
    this._identityPopup.hidden = false;

    // Remove the reload hint that we show after a user has cleared a permission.
    this._permissionReloadHint.setAttribute("hidden", "true");

    // Update the popup strings
    this.refreshIdentityPopup();

    // Add the "open" attribute to the identity box for styling
    this._identityBox.setAttribute("open", "true");

    // Check the panel state of the protections panel. Hide it if needed.
    if (gProtectionsHandler._protectionsPopup.state != "closed") {
      PanelMultiView.hidePopup(gProtectionsHandler._protectionsPopup);
    }

    // Now open the popup, anchored off the primary chrome element
    PanelMultiView.openPopup(this._identityPopup, this._identityIcon, {
      position: "bottomcenter topleft",
      triggerEvent: event,
    }).catch(Cu.reportError);
  },

  onPopupShown(event) {
    if (event.target == this._identityPopup) {
      window.addEventListener("focus", this, true);
    }

    Services.telemetry.recordEvent(
      "security.ui.identitypopup",
      "open",
      "identity_popup"
    );
  },

  onPopupHidden(event) {
    if (event.target == this._identityPopup) {
      window.removeEventListener("focus", this, true);
      this._identityBox.removeAttribute("open");
    }
  },

  handleEvent(event) {
    let elem = document.activeElement;
    let position = elem.compareDocumentPosition(this._identityPopup);

    if (
      !(
        position &
        (Node.DOCUMENT_POSITION_CONTAINS | Node.DOCUMENT_POSITION_CONTAINED_BY)
      ) &&
      !this._identityPopup.hasAttribute("noautohide")
    ) {
      // Hide the panel when focusing an element that is
      // neither an ancestor nor descendant unless the panel has
      // @noautohide (e.g. for a tour).
      PanelMultiView.hidePopup(this._identityPopup);
    }
  },

  observe(subject, topic, data) {
    // Exclude permissions which do not appear in the UI in order to avoid
    // doing extra work here.
    if (
      topic == "perm-changed" &&
      subject &&
      SitePermissions.listPermissions().includes(
        subject.QueryInterface(Ci.nsIPermission).type
      )
    ) {
      this.refreshIdentityBlock();
    }
  },

  onDragStart(event) {
    if (gURLBar.getAttribute("pageproxystate") != "valid") {
      return;
    }

    let value = gBrowser.currentURI.displaySpec;
    let urlString = value + "\n" + gBrowser.contentTitle;
    let htmlString = '<a href="' + value + '">' + value + "</a>";

    let windowUtils = window.windowUtils;
    let scale = windowUtils.screenPixelsPerCSSPixel / windowUtils.fullZoom;
    let canvas = document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "canvas"
    );
    canvas.width = 550 * scale;
    let ctx = canvas.getContext("2d");
    ctx.font = `${14 * scale}px sans-serif`;
    ctx.fillText(`${value}`, 20 * scale, 14 * scale);
    let tabIcon = gBrowser.selectedTab.iconImage;
    let image = new Image();
    image.src = tabIcon.src;
    ctx.drawImage(image, 0, 0, 16 * scale, 16 * scale);

    let dt = event.dataTransfer;
    dt.setData("text/x-moz-url", urlString);
    dt.setData("text/uri-list", value);
    dt.setData("text/plain", value);
    dt.setData("text/html", htmlString);
    dt.setDragImage(canvas, 16, 16);
  },

  onLocationChange() {
    this._permissionReloadHint.setAttribute("hidden", "true");

    if (!this._permissionList.hasChildNodes()) {
      this._permissionEmptyHint.removeAttribute("hidden");
    }
  },

  _updateAttribute(elem, attr, value) {
    if (value) {
      elem.setAttribute(attr, value);
    } else {
      elem.removeAttribute(attr);
    }
  },

  updateSitePermissions() {
    while (this._permissionList.hasChildNodes()) {
      this._permissionList.removeChild(this._permissionList.lastChild);
    }

    let permissions = SitePermissions.getAllPermissionDetailsForBrowser(
      gBrowser.selectedBrowser
    );

    if (this._sharingState && this._sharingState.geo) {
      let geoPermission = permissions.find(perm => perm.id === "geo");
      if (geoPermission) {
        geoPermission.sharingState = true;
      } else {
        permissions.push({
          id: "geo",
          state: SitePermissions.ALLOW,
          scope: SitePermissions.SCOPE_REQUEST,
          sharingState: true,
        });
      }
    }

    if (this._sharingState && this._sharingState.webRTC) {
      let webrtcState = this._sharingState.webRTC;
      // If WebRTC device or screen permissions are in use, we need to find
      // the associated permission item to set the sharingState field.
      for (let id of ["camera", "microphone", "screen"]) {
        if (webrtcState[id]) {
          let found = false;
          for (let permission of permissions) {
            if (permission.id != id) {
              continue;
            }
            found = true;
            permission.sharingState = webrtcState[id];
            break;
          }
          if (!found) {
            // If the permission item we were looking for doesn't exist,
            // the user has temporarily allowed sharing and we need to add
            // an item in the permissions array to reflect this.
            permissions.push({
              id,
              state: SitePermissions.ALLOW,
              scope: SitePermissions.SCOPE_REQUEST,
              sharingState: webrtcState[id],
            });
          }
        }
      }
    }

    let hasBlockedPopupIndicator = false;
    for (let permission of permissions) {
      if (permission.id == "storage-access") {
        // Ignore storage access permissions here, they are made visible inside
        // the Content Blocking UI.
        continue;
      }
      let item = this._createPermissionItem(permission);
      if (!item) {
        continue;
      }
      this._permissionList.appendChild(item);

      if (
        permission.id == "popup" &&
        gBrowser.selectedBrowser.blockedPopups &&
        gBrowser.selectedBrowser.blockedPopups.length
      ) {
        this._createBlockedPopupIndicator();
        hasBlockedPopupIndicator = true;
      } else if (
        permission.id == "geo" &&
        permission.state === SitePermissions.ALLOW
      ) {
        this._createGeoLocationLastAccessIndicator();
      }
    }

    if (
      gBrowser.selectedBrowser.blockedPopups &&
      gBrowser.selectedBrowser.blockedPopups.length &&
      !hasBlockedPopupIndicator
    ) {
      let permission = {
        id: "popup",
        state: SitePermissions.getDefault("popup"),
        scope: SitePermissions.SCOPE_PERSISTENT,
      };
      let item = this._createPermissionItem(permission);
      this._permissionList.appendChild(item);
      this._createBlockedPopupIndicator();
    }

    // Show a placeholder text if there's no permission and no reload hint.
    if (
      !this._permissionList.hasChildNodes() &&
      this._permissionReloadHint.hasAttribute("hidden")
    ) {
      this._permissionEmptyHint.removeAttribute("hidden");
    } else {
      this._permissionEmptyHint.setAttribute("hidden", "true");
    }
  },

  _createPermissionItem(aPermission) {
    let container = document.createXULElement("hbox");
    container.setAttribute("class", "identity-popup-permission-item");
    container.setAttribute("align", "center");
    container.setAttribute("role", "group");

    let img = document.createXULElement("image");
    img.classList.add(
      "identity-popup-permission-icon",
      aPermission.id + "-icon"
    );
    if (
      aPermission.state == SitePermissions.BLOCK ||
      aPermission.state == SitePermissions.AUTOPLAY_BLOCKED_ALL
    ) {
      img.classList.add("blocked-permission-icon");
    }

    if (
      aPermission.sharingState ==
        Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED ||
      (aPermission.id == "screen" &&
        aPermission.sharingState &&
        !aPermission.sharingState.includes("Paused"))
    ) {
      img.classList.add("in-use");

      // Synchronize control center and identity block blinking animations.
      window
        .promiseDocumentFlushed(() => {
          let sharingIconBlink = this._webRTCSharingIcon.getAnimations()[0];
          let imgBlink = img.getAnimations()[0];
          return [sharingIconBlink, imgBlink];
        })
        .then(([sharingIconBlink, imgBlink]) => {
          if (sharingIconBlink && imgBlink) {
            imgBlink.startTime = sharingIconBlink.startTime;
          }
        });
    }

    let nameLabel = document.createXULElement("label");
    nameLabel.setAttribute("flex", "1");
    nameLabel.setAttribute("class", "identity-popup-permission-label");
    let label = SitePermissions.getPermissionLabel(aPermission.id);
    if (label === null) {
      return null;
    }
    nameLabel.textContent = label;
    let nameLabelId = "identity-popup-permission-label-" + aPermission.id;
    nameLabel.setAttribute("id", nameLabelId);

    let isPolicyPermission = [
      SitePermissions.SCOPE_POLICY,
      SitePermissions.SCOPE_GLOBAL,
    ].includes(aPermission.scope);

    if (
      (aPermission.id == "popup" && !isPolicyPermission) ||
      aPermission.id == "autoplay-media"
    ) {
      let menulist = document.createXULElement("menulist");
      let menupopup = document.createXULElement("menupopup");
      let block = document.createXULElement("vbox");
      block.setAttribute("id", "identity-popup-popup-container");
      menulist.setAttribute("sizetopopup", "none");
      menulist.setAttribute("class", "identity-popup-popup-menulist");
      menulist.setAttribute("id", "identity-popup-popup-menulist");

      for (let state of SitePermissions.getAvailableStates(aPermission.id)) {
        let menuitem = document.createXULElement("menuitem");
        // We need to correctly display the default/unknown state, which has its
        // own integer value (0) but represents one of the other states.
        if (state == SitePermissions.getDefault(aPermission.id)) {
          menuitem.setAttribute("value", "0");
        } else {
          menuitem.setAttribute("value", state);
        }

        menuitem.setAttribute(
          "label",
          SitePermissions.getMultichoiceStateLabel(aPermission.id, state)
        );
        menupopup.appendChild(menuitem);
      }

      menulist.appendChild(menupopup);

      if (aPermission.state == SitePermissions.getDefault(aPermission.id)) {
        menulist.value = "0";
      } else {
        menulist.value = aPermission.state;
      }

      // Avoiding listening to the "select" event on purpose. See Bug 1404262.
      menulist.addEventListener("command", () => {
        SitePermissions.setForPrincipal(
          gBrowser.contentPrincipal,
          aPermission.id,
          menulist.selectedItem.value
        );
      });

      container.appendChild(img);
      container.appendChild(nameLabel);
      container.appendChild(menulist);
      container.setAttribute("aria-labelledby", nameLabelId);
      block.appendChild(container);

      return block;
    }

    let stateLabel = document.createXULElement("label");
    stateLabel.setAttribute("flex", "1");
    stateLabel.setAttribute("class", "identity-popup-permission-state-label");
    let stateLabelId =
      "identity-popup-permission-state-label-" + aPermission.id;
    stateLabel.setAttribute("id", stateLabelId);
    let { state, scope } = aPermission;
    // If the user did not permanently allow this device but it is currently
    // used, set the variables to display a "temporarily allowed" info.
    if (state != SitePermissions.ALLOW && aPermission.sharingState) {
      state = SitePermissions.ALLOW;
      scope = SitePermissions.SCOPE_REQUEST;
    }
    stateLabel.textContent = SitePermissions.getCurrentStateLabel(
      state,
      aPermission.id,
      scope
    );

    container.appendChild(img);
    container.appendChild(nameLabel);
    container.appendChild(stateLabel);
    container.setAttribute("aria-labelledby", nameLabelId + " " + stateLabelId);

    /* We return the permission item here without a remove button if the permission is a
       SCOPE_POLICY or SCOPE_GLOBAL permission. Policy permissions cannot be
       removed/changed for the duration of the browser session. */
    if (isPolicyPermission) {
      return container;
    }

    if (aPermission.id == "geo") {
      let block = document.createXULElement("vbox");
      block.setAttribute("id", "identity-popup-geo-container");

      let button = this._createPermissionClearButton(aPermission, block);
      container.appendChild(button);

      block.appendChild(container);
      return block;
    }

    let button = this._createPermissionClearButton(aPermission, container);
    container.appendChild(button);

    return container;
  },

  _createPermissionClearButton(aPermission, container) {
    let button = document.createXULElement("button");
    button.setAttribute("class", "identity-popup-permission-remove-button");
    let tooltiptext = gNavigatorBundle.getString("permissions.remove.tooltip");
    button.setAttribute("tooltiptext", tooltiptext);
    button.addEventListener("command", () => {
      let browser = gBrowser.selectedBrowser;
      this._permissionList.removeChild(container);
      if (
        aPermission.sharingState &&
        ["camera", "microphone", "screen"].includes(aPermission.id)
      ) {
        let windowId = this._sharingState.webRTC.windowId;
        if (aPermission.id == "screen") {
          windowId = "screen:" + windowId;
        } else {
          // If we set persistent permissions or the sharing has
          // started due to existing persistent permissions, we need
          // to handle removing these even for frames with different hostnames.
          let principals = browser._devicePermissionPrincipals || [];
          for (let principal of principals) {
            // It's not possible to stop sharing one of camera/microphone
            // without the other.
            for (let id of ["camera", "microphone"]) {
              if (this._sharingState.webRTC[id]) {
                let perm = SitePermissions.getForPrincipal(principal, id);
                if (
                  perm.state == SitePermissions.ALLOW &&
                  perm.scope == SitePermissions.SCOPE_PERSISTENT
                ) {
                  SitePermissions.removeFromPrincipal(principal, id);
                }
              }
            }
          }
        }
        browser.messageManager.sendAsyncMessage("webrtc:StopSharing", windowId);
        webrtcUI.forgetActivePermissionsFromBrowser(gBrowser.selectedBrowser);
      }
      SitePermissions.removeFromPrincipal(
        gBrowser.contentPrincipal,
        aPermission.id,
        browser
      );

      this._permissionReloadHint.removeAttribute("hidden");
      PanelView.forNode(
        this._identityPopupMainView
      ).descriptionHeightWorkaround();

      if (aPermission.id === "geo") {
        gBrowser.updateBrowserSharing(browser, { geo: false });
      }
    });

    return button;
  },

  _getGeoLocationLastAccess() {
    return new Promise(resolve => {
      let lastAccess = null;
      ContentPrefService2.getByDomainAndName(
        gBrowser.currentURI.spec,
        "permissions.geoLocation.lastAccess",
        gBrowser.selectedBrowser.loadContext,
        {
          handleResult(pref) {
            lastAccess = pref.value;
          },
          handleCompletion() {
            resolve(lastAccess);
          },
        }
      );
    });
  },

  async _createGeoLocationLastAccessIndicator() {
    let lastAccessStr = await this._getGeoLocationLastAccess();

    if (lastAccessStr == null) {
      return;
    }
    let lastAccess = new Date(lastAccessStr);
    if (isNaN(lastAccess)) {
      Cu.reportError("Invalid timestamp for last geolocation access");
      return;
    }

    let icon = document.createXULElement("image");
    icon.setAttribute("class", "popup-subitem");

    let indicator = document.createXULElement("hbox");
    indicator.setAttribute("class", "identity-popup-permission-item");
    indicator.setAttribute("align", "center");
    indicator.setAttribute("id", "geo-access-indicator-item");

    let timeFormat = new Services.intl.RelativeTimeFormat(undefined, {});

    let text = document.createXULElement("label");
    text.setAttribute("flex", "1");
    text.setAttribute("class", "identity-popup-permission-label");

    text.textContent = gNavigatorBundle.getFormattedString(
      "geolocationLastAccessIndicatorText",
      [timeFormat.formatBestUnit(lastAccess)]
    );

    indicator.appendChild(icon);
    indicator.appendChild(text);

    document
      .getElementById("identity-popup-geo-container")
      .appendChild(indicator);
  },

  _createBlockedPopupIndicator() {
    let indicator = document.createXULElement("hbox");
    indicator.setAttribute("class", "identity-popup-permission-item");
    indicator.setAttribute("align", "center");
    indicator.setAttribute("id", "blocked-popup-indicator-item");

    let icon = document.createXULElement("image");
    icon.setAttribute("class", "popup-subitem");

    let text = document.createXULElement("label", { is: "text-link" });
    text.setAttribute("flex", "1");
    text.setAttribute("class", "identity-popup-permission-label");

    let popupCount = gBrowser.selectedBrowser.blockedPopups.length;
    let messageBase = gNavigatorBundle.getString(
      "popupShowBlockedPopupsIndicatorText"
    );
    let message = PluralForm.get(popupCount, messageBase).replace(
      "#1",
      popupCount
    );
    text.textContent = message;

    text.addEventListener("click", () => {
      gPopupBlockerObserver.showAllBlockedPopups(gBrowser.selectedBrowser);
    });

    indicator.appendChild(icon);
    indicator.appendChild(text);

    document
      .getElementById("identity-popup-popup-container")
      .appendChild(indicator);
  },
};
