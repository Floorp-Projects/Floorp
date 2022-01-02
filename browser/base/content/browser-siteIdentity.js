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
   * processed by createExposableURI.
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

  get _isContentHttpsOnlyModeUpgraded() {
    return (
      this._state & Ci.nsIWebProgressListener.STATE_HTTPS_ONLY_MODE_UPGRADED
    );
  },

  get _isContentHttpsOnlyModeUpgradeFailed() {
    return (
      this._state &
      Ci.nsIWebProgressListener.STATE_HTTPS_ONLY_MODE_UPGRADE_FAILED
    );
  },

  get _isCertUserOverridden() {
    return this._state & Ci.nsIWebProgressListener.STATE_CERT_USER_OVERRIDDEN;
  },

  get _isCertErrorPage() {
    let { documentURI } = gBrowser.selectedBrowser;
    if (documentURI?.scheme != "about") {
      return false;
    }

    return (
      documentURI.filePath == "certerror" ||
      (documentURI.filePath == "neterror" &&
        new URLSearchParams(documentURI.query).get("e") == "nssFailure2")
    );
  },

  get _isAboutNetErrorPage() {
    let { documentURI } = gBrowser.selectedBrowser;
    return documentURI?.scheme == "about" && documentURI.filePath == "neterror";
  },

  get _isAboutHttpsOnlyErrorPage() {
    let { documentURI } = gBrowser.selectedBrowser;
    return (
      documentURI?.scheme == "about" && documentURI.filePath == "httpsonlyerror"
    );
  },

  get _isPDFViewer() {
    return gBrowser.contentPrincipal?.originNoSuffix == "resource://pdf.js";
  },

  get _isPotentiallyTrustworthy() {
    // For PDF viewer pages (pdf.js) we can't rely on the isSecureContext
    // field. The backend will return isSecureContext = true, because the
    // content principal has a resource:// URI. Since we don't check
    // isSecureContext for PDF viewer pages anymore, otherwise secure
    // contexts, such as a localhost, will me marked as insecure when showing
    // PDFs.
    return (
      !this._isBrokenConnection &&
      !this._isPDFViewer &&
      (this._isSecureContext ||
        gBrowser.selectedBrowser.documentURI?.scheme == "chrome")
    );
  },

  get _isAboutBlockedPage() {
    let { documentURI } = gBrowser.selectedBrowser;
    return documentURI?.scheme == "about" && documentURI.filePath == "blocked";
  },

  _popupInitialized: false,
  _initializePopup() {
    if (!this._popupInitialized) {
      let wrapper = document.getElementById("template-identity-popup");
      wrapper.replaceWith(wrapper.content);
      this._popupInitialized = true;
    }
  },

  hidePopup() {
    if (this._popupInitialized) {
      PanelMultiView.hidePopup(this._identityPopup);
    }
  },

  // smart getters
  get _identityPopup() {
    if (!this._popupInitialized) {
      return null;
    }
    delete this._identityPopup;
    return (this._identityPopup = document.getElementById("identity-popup"));
  },
  get _identityBox() {
    delete this._identityBox;
    return (this._identityBox = document.getElementById("identity-box"));
  },
  get _identityIconBox() {
    delete this._identityIconBox;
    return (this._identityIconBox = document.getElementById(
      "identity-icon-box"
    ));
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
  get _identityPopupSecurityView() {
    delete this._identityPopupSecurityView;
    return (this._identityPopupSecurityView = document.getElementById(
      "identity-popup-securityView"
    ));
  },
  get _identityPopupHttpsOnlyModeMenuList() {
    delete this._identityPopupHttpsOnlyModeMenuList;
    return (this._identityPopupHttpsOnlyModeMenuList = document.getElementById(
      "identity-popup-security-httpsonlymode-menulist"
    ));
  },
  get _identityPopupHttpsOnlyModeMenuListTempItem() {
    delete this._identityPopupHttpsOnlyModeMenuListTempItem;
    return (this._identityPopupHttpsOnlyModeMenuListTempItem = document.getElementById(
      "identity-popup-security-menulist-tempitem"
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
  get _identityIcon() {
    delete this._identityIcon;
    return (this._identityIcon = document.getElementById("identity-icon"));
  },
  get _clearSiteDataFooter() {
    delete this._clearSiteDataFooter;
    return (this._clearSiteDataFooter = document.getElementById(
      "identity-popup-clear-sitedata-footer"
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
  get _httpsOnlyModeEnabled() {
    delete this._httpsOnlyModeEnabled;
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_httpsOnlyModeEnabled",
      "dom.security.https_only_mode"
    );
    return this._httpsOnlyModeEnabled;
  },
  get _httpsOnlyModeEnabledPBM() {
    delete this._httpsOnlyModeEnabledPBM;
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_httpsOnlyModeEnabledPBM",
      "dom.security.https_only_mode_pbm"
    );
    return this._httpsOnlyModeEnabledPBM;
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

    // Hide the popup before showing the removal prompt, to
    // avoid a pretty ugly transition. Also hide it even
    // if the update resulted in no site data, to keep the
    // illusion that clicking the button had an effect.
    let hidden = new Promise(c => {
      this._identityPopup.addEventListener("popuphidden", c, { once: true });
    });
    PanelMultiView.hidePopup(this._identityPopup);
    await hidden;

    let baseDomain = SiteDataManager.getBaseDomainFromHost(this._uri.host);
    if (SiteDataManager.promptSiteDataRemoval(window, [baseDomain])) {
      SiteDataManager.remove(baseDomain);
    }

    event.stopPropagation();
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
      document.getElementById("identity-popup-security-button")
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

    SitePermissions.setForPrincipal(
      gBrowser.contentPrincipal,
      "mixed-content",
      SitePermissions.ALLOW,
      SitePermissions.SCOPE_SESSION
    );

    // Reload the page with the content unblocked
    BrowserReloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE);
    if (this._popupInitialized) {
      PanelMultiView.hidePopup(this._identityPopup);
    }
  },

  // This is needed for some tests which need the permission reset, but which
  // then reuse the browser and would race between the reload and the next
  // load.
  enableMixedContentProtectionNoReload() {
    this.enableMixedContentProtection(false);
  },

  enableMixedContentProtection(reload = true) {
    SitePermissions.removeFromPrincipal(
      gBrowser.contentPrincipal,
      "mixed-content"
    );
    if (reload) {
      BrowserReload();
    }
    if (this._popupInitialized) {
      PanelMultiView.hidePopup(this._identityPopup);
    }
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
    this._overrideService.clearValidityOverride(
      host,
      port,
      gBrowser.contentPrincipal.originAttributes
    );
    BrowserReloadSkipCache();
    if (this._popupInitialized) {
      PanelMultiView.hidePopup(this._identityPopup);
    }
  },

  /**
   * Gets the current HTTPS-Only mode permission for the current page.
   * Values are the same as in #identity-popup-security-httpsonlymode-menulist
   */
  _getHttpsOnlyPermission() {
    const { state } = SitePermissions.getForPrincipal(
      gBrowser.contentPrincipal,
      "https-only-load-insecure"
    );
    switch (state) {
      case Ci.nsIHttpsOnlyModePermission.LOAD_INSECURE_ALLOW_SESSION:
        return 2; // Off temporarily
      case Ci.nsIHttpsOnlyModePermission.LOAD_INSECURE_ALLOW:
        return 1; // Off
      default:
        return 0; // On
    }
  },

  /**
   * Sets/removes HTTPS-Only Mode exception and possibly reloads the page.
   */
  changeHttpsOnlyPermission() {
    // Get the new value from the menulist and the current value
    // Note: value and permission association is laid out
    //       in _getHttpsOnlyPermission
    const oldValue = this._getHttpsOnlyPermission();
    let newValue = parseInt(
      this._identityPopupHttpsOnlyModeMenuList.selectedItem.value,
      10
    );

    // If nothing changed, just return here
    if (newValue === oldValue) {
      return;
    }

    // Permissions set in PMB get deleted anyway, but to make sure, let's make
    // the permission session-only.
    if (newValue === 1 && PrivateBrowsingUtils.isWindowPrivate(window)) {
      newValue = 2;
    }

    // Usually we want to set the permission for the current site and therefore
    // the current principal...
    let principal = gBrowser.contentPrincipal;
    // ...but if we're on the HTTPS-Only error page, the content-principal is
    // for HTTPS but. We always want to set the exception for HTTP. (Code should
    // be almost identical to the one in AboutHttpsOnlyErrorParent.jsm)
    let newURI;
    if (this._isAboutHttpsOnlyErrorPage) {
      newURI = gBrowser.currentURI
        .mutate()
        .setScheme("http")
        .finalize();
      principal = Services.scriptSecurityManager.createContentPrincipal(
        newURI,
        gBrowser.contentPrincipal.originAttributes
      );
    }

    // Set or remove the permission
    if (newValue === 0) {
      SitePermissions.removeFromPrincipal(
        principal,
        "https-only-load-insecure"
      );
    } else if (newValue === 1) {
      SitePermissions.setForPrincipal(
        principal,
        "https-only-load-insecure",
        Ci.nsIHttpsOnlyModePermission.LOAD_INSECURE_ALLOW,
        SitePermissions.SCOPE_PERSISTENT
      );
    } else {
      SitePermissions.setForPrincipal(
        principal,
        "https-only-load-insecure",
        Ci.nsIHttpsOnlyModePermission.LOAD_INSECURE_ALLOW_SESSION,
        SitePermissions.SCOPE_SESSION
      );
    }

    // If we're on the error-page, we have to redirect the user
    // from HTTPS to HTTP. Otherwise we can just reload the page.
    if (this._isAboutHttpsOnlyErrorPage) {
      gBrowser.loadURI(newURI.spec, {
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
        loadFlags: Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY,
      });
      if (this._popupInitialized) {
        PanelMultiView.hidePopup(this._identityPopup);
      }
      return;
    }
    // The page only needs to reload if we switch between allow and block
    // Because "off" is 1 and "off temporarily" is 2, we can just check if the
    // sum of newValue and oldValue is 3.
    if (newValue + oldValue !== 3) {
      BrowserReloadSkipCache();
      if (this._popupInitialized) {
        PanelMultiView.hidePopup(this._identityPopup);
      }
      return;
    }
    // Otherwise we just refresh the interface
    this.refreshIdentityPopup();
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
   *        processed by createExposableURI.
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
      this.hidePopup();
      gPermissionPanel.hidePopup();
    }

    // NOTE: We do NOT update the identity popup (the control center) when
    // we receive a new security state on the existing page (i.e. from a
    // subframe). If the user opened the popup and looks at the provided
    // information we don't want to suddenly change the panel contents.
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

    if (this._uri.schemeIs("about")) {
      // For example in about:certificate the original URL is
      // about:certificate?cert=<large base64 encoded data>&cert=<large base64 encoded data>&cert=...
      // So, instead of showing that large string in the identity panel header, we are just showing
      // about:certificate now. For the other about pages we are just showing about:<page>
      host = "about:" + this._uri.filePath;
    }

    if (this._uri.schemeIs("chrome")) {
      host = this._uri.spec;
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
    issuerCert = this._secInfo.succeededCertChain[
      this._secInfo.succeededCertChain.length - 1
    ];

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
   * Updates the security identity in the identity block.
   */
  _refreshIdentityIcons() {
    let icon_label = "";
    let tooltip = "";

    if (this._isSecureInternalUI) {
      // This is a secure internal Firefox page.
      this._identityBox.className = "chromeUI";
      let brandBundle = document.getElementById("bundle_brand");
      icon_label = brandBundle.getString("brandShorterName");
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
    } else if (this._isCertErrorPage) {
      // We show a warning lock icon for certificate errors, and
      // show the "Not Secure" text.
      this._identityBox.className = "certErrorPage notSecureText";
      icon_label = gNavigatorBundle.getString("identity.notSecure.label");
      tooltip = gNavigatorBundle.getString("identity.notSecure.tooltip");
    } else if (this._isAboutHttpsOnlyErrorPage) {
      // We show a not secure lock icon for 'about:httpsonlyerror' page.
      this._identityBox.className = "httpsOnlyErrorPage";
    } else if (this._isAboutNetErrorPage || this._isAboutBlockedPage) {
      // Network errors and blocked pages get a more neutral icon
      this._identityBox.className = "unknownIdentity";
    } else if (this._isPotentiallyTrustworthy) {
      // This is a local resource (and shouldn't be marked insecure).
      this._identityBox.className = "localResource";
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
    }

    if (this._isCertUserOverridden) {
      this._identityBox.classList.add("certUserOverridden");
      // Cert is trusted because of a security exception, verifier is a special string.
      tooltip = gNavigatorBundle.getString(
        "identity.identified.verified_by_you"
      );
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

    this._identityIconLabel.setAttribute("tooltiptext", tooltip);
    this._identityIconLabel.setAttribute("value", icon_label);
    this._identityIconLabel.collapsed = !icon_label;
  },

  /**
   * Updates the identity block user interface with the data from this object.
   */
  refreshIdentityBlock() {
    if (!this._identityBox) {
      return;
    }

    this._refreshIdentityIcons();

    // If this condition is true, the URL bar will have an "invalid"
    // pageproxystate, so we should hide the permission icons.
    if (this._hasInvalidPageProxyState()) {
      gPermissionPanel.hidePermissionIcons();
    } else {
      gPermissionPanel.refreshPermissionIcons();
    }

    // Hide the shield icon if it is a chrome page.
    gProtectionsHandler._trackingProtectionIconContainer.classList.toggle(
      "chromeUI",
      this._isSecureInternalUI
    );
  },

  /**
   * Set up the title and content messages for the identity message popup,
   * based on the specified mode, and the details of the SSL cert, where
   * applicable
   */
  refreshIdentityPopup() {
    // Update cookies and site data information and show the
    // "Clear Site Data" button if the site is storing local data, and
    // if the page is not controlled by a WebExtension.
    this._clearSiteDataFooter.hidden = true;
    let securityButton = document.getElementById(
      "identity-popup-security-button"
    );
    securityButton.removeAttribute("footerHidden");
    if (this._uriHasHost && !this._pageExtensionPolicy) {
      SiteDataManager.hasSiteData(this._uri.asciiHost).then(hasData => {
        this._clearSiteDataFooter.hidden = !hasData;
        securityButton.setAttribute("footerHidden", !hasData);
      });
    }

    // Update "Learn More" for Mixed Content Blocking and Insecure Login Forms.
    let baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");
    this._identityPopupMixedContentLearnMore.forEach(e =>
      e.setAttribute("href", baseURL + "mixed-content")
    );

    this._identityPopupCustomRootLearnMore.setAttribute(
      "href",
      baseURL + "enterprise-roots"
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
    } else if (this._isCertErrorPage) {
      connection = "cert-error-page";
    } else if (this._isAboutHttpsOnlyErrorPage) {
      connection = "https-only-error-page";
    } else if (this._isAboutBlockedPage) {
      connection = "not-secure";
    } else if (this._isAboutNetErrorPage) {
      connection = "net-error-page";
    } else if (this._isPotentiallyTrustworthy) {
      connection = "file";
    }

    let securityButtonNode = document.getElementById(
      "identity-popup-security-button"
    );

    let disableSecurityButton = ![
      "not-secure",
      "secure",
      "secure-ev",
      "secure-cert-user-overridden",
      "cert-error-page",
      "net-error-page",
      "https-only-error-page",
    ].includes(connection);
    if (disableSecurityButton) {
      securityButtonNode.disabled = true;
      securityButtonNode.classList.remove("subviewbutton-nav");
    } else {
      securityButtonNode.disabled = false;
      securityButtonNode.classList.add("subviewbutton-nav");
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

    // If HTTPS-Only Mode is enabled, check the permission status
    const privateBrowsingWindow = PrivateBrowsingUtils.isWindowPrivate(window);
    let httpsOnlyStatus = "";
    if (
      this._httpsOnlyModeEnabled ||
      (privateBrowsingWindow && this._httpsOnlyModeEnabledPBM)
    ) {
      // Note: value and permission association is laid out
      //       in _getHttpsOnlyPermission
      let value = this._getHttpsOnlyPermission();

      // Because everything in PBM is temporary anyway, we don't need to make the distinction
      if (privateBrowsingWindow) {
        if (value === 2) {
          value = 1;
        }
        // Hide "off temporarily" option
        this._identityPopupHttpsOnlyModeMenuListTempItem.style.display = "none";
      } else {
        this._identityPopupHttpsOnlyModeMenuListTempItem.style.display = "";
      }

      this._identityPopupHttpsOnlyModeMenuList.value = value;

      if (value > 0) {
        httpsOnlyStatus = "exception";
      } else if (this._isAboutHttpsOnlyErrorPage) {
        httpsOnlyStatus = "failed-top";
      } else if (this._isContentHttpsOnlyModeUpgradeFailed) {
        httpsOnlyStatus = "failed-sub";
      } else if (this._isContentHttpsOnlyModeUpgraded) {
        httpsOnlyStatus = "upgraded";
      }
    }

    // Update all elements.
    let elementIDs = ["identity-popup", "identity-popup-securityView-body"];

    for (let id of elementIDs) {
      let element = document.getElementById(id);
      this._updateAttribute(element, "connection", connection);
      this._updateAttribute(element, "ciphers", ciphers);
      this._updateAttribute(element, "mixedcontent", mixedcontent);
      this._updateAttribute(element, "isbroken", this._isBrokenConnection);
      this._updateAttribute(element, "customroot", customRoot);
      this._updateAttribute(element, "httpsonlystatus", httpsOnlyStatus);
    }

    // Initialize the optional strings to empty values
    let supplemental = "";
    let verifier = "";
    let host = this.getHostForDisplay();
    let owner = "";

    // Fill in the CA name if we have a valid TLS certificate.
    if (this._isSecureConnection || this._isCertUserOverridden) {
      verifier = this._identityIconLabel.tooltipText;
    }

    // Fill in organization information if we have a valid EV certificate.
    if (this._isEV) {
      let iData = this.getIdentityData();
      owner = iData.subjectOrg;
      verifier = this._identityIconLabel.tooltipText;

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
    document.l10n.setAttributes(
      this._identityPopupMainViewHeaderLabel,
      "identity-site-information",
      {
        host,
      }
    );

    document.l10n.setAttributes(
      this._identityPopupSecurityView,
      "identity-header-security-with-host",
      {
        host,
      }
    );

    this._identityPopupSecurityEVContentOwner.textContent = gNavigatorBundle.getFormattedString(
      "identity.ev.contentOwner2",
      [owner]
    );

    this._identityPopupContentOwner.textContent = owner;
    this._identityPopupContentSupp.textContent = supplemental;
    this._identityPopupContentVerif.textContent = verifier;
  },

  setURI(uri) {
    if (uri.schemeIs("view-source")) {
      uri = Services.io.newURI(uri.spec.replace(/^view-source:/i, ""));
    }
    this._uri = uri;

    try {
      // Account for file: urls and catch when "" is the value
      this._uriHasHost = !!this._uri.host;
    } catch (ex) {
      this._uriHasHost = false;
    }

    if (uri.schemeIs("about")) {
      let module = E10SUtils.getAboutModule(uri);
      if (module) {
        let flags = module.getURIFlags(uri);
        this._isSecureInternalUI = !!(
          flags & Ci.nsIAboutModule.IS_SECURE_CHROME_UI
        );
      }
    } else {
      this._isSecureInternalUI = false;
    }
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

    // Don't allow left click, space or enter if the location has been modified.
    if (gURLBar.getAttribute("pageproxystate") != "valid") {
      return;
    }

    this._openPopup(event);
  },

  _openPopup(event) {
    // Make the popup available.
    this._initializePopup();

    // Update the popup strings
    this.refreshIdentityPopup();

    // Check the panel state of other panels. Hide them if needed.
    let openPanels = Array.from(document.querySelectorAll("panel[openpanel]"));
    for (let panel of openPanels) {
      PanelMultiView.hidePopup(panel);
    }

    // Now open the popup, anchored off the primary chrome element
    PanelMultiView.openPopup(this._identityPopup, this._identityIconBox, {
      position: "bottomcenter topleft",
      triggerEvent: event,
    }).catch(Cu.reportError);
  },

  onPopupShown(event) {
    if (event.target == this._identityPopup) {
      PopupNotifications.suppressWhileOpen(this._identityPopup);
      window.addEventListener("focus", this, true);
    }
  },

  onPopupHidden(event) {
    if (event.target == this._identityPopup) {
      window.removeEventListener("focus", this, true);
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
    switch (topic) {
      case "perm-changed": {
        // Exclude permissions which do not appear in the UI in order to avoid
        // doing extra work here.
        if (!subject) {
          return;
        }
        let { type } = subject.QueryInterface(Ci.nsIPermission);
        if (SitePermissions.isSitePermission(type)) {
          this.refreshIdentityBlock();
        }
        break;
      }
    }
  },

  onDragStart(event) {
    const TEXT_SIZE = 14;
    const IMAGE_SIZE = 16;
    const SPACING = 5;

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
    ctx.font = `${TEXT_SIZE * scale}px sans-serif`;
    let tabIcon = gBrowser.selectedTab.iconImage;
    let image = new Image();
    image.src = tabIcon.src;
    let textWidth = ctx.measureText(value).width / scale;
    let textHeight = parseInt(ctx.font, 10) / scale;
    let imageHorizontalOffset, imageVerticalOffset;
    imageHorizontalOffset = imageVerticalOffset = SPACING;
    let textHorizontalOffset = image.width ? IMAGE_SIZE + SPACING * 2 : SPACING;
    let textVerticalOffset = textHeight + SPACING - 1;
    let backgroundColor = "white";
    let textColor = "black";
    let totalWidth = image.width
      ? textWidth + IMAGE_SIZE + 3 * SPACING
      : textWidth + 2 * SPACING;
    let totalHeight = image.width
      ? IMAGE_SIZE + 2 * SPACING
      : textHeight + 2 * SPACING;
    ctx.fillStyle = backgroundColor;
    ctx.fillRect(0, 0, totalWidth * scale, totalHeight * scale);
    ctx.fillStyle = textColor;
    ctx.fillText(
      `${value}`,
      textHorizontalOffset * scale,
      textVerticalOffset * scale
    );
    try {
      ctx.drawImage(
        image,
        imageHorizontalOffset * scale,
        imageVerticalOffset * scale,
        IMAGE_SIZE * scale,
        IMAGE_SIZE * scale
      );
    } catch (e) {
      // Sites might specify invalid data URIs favicons that
      // will result in errors when trying to draw, we can
      // just ignore this case and not paint any favicon.
    }

    let dt = event.dataTransfer;
    dt.setData("text/x-moz-url", urlString);
    dt.setData("text/uri-list", value);
    dt.setData("text/plain", value);
    dt.setData("text/html", htmlString);
    dt.setDragImage(canvas, 16, 16);

    // Don't cover potential drop targets on the toolbars or in content.
    gURLBar.view.close();
  },

  _updateAttribute(elem, attr, value) {
    if (value) {
      elem.setAttribute(attr, value);
    } else {
      elem.removeAttribute(attr);
    }
  },
};
