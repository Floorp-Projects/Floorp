/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/BrowserUtils.jsm");

/* import-globals-from pageInfo.js */

XPCOMUtils.defineLazyModuleGetter(this, "LoginHelper",
                                  "resource://gre/modules/LoginHelper.jsm");

var security = {
  init(uri, windowInfo) {
    this.uri = uri;
    this.windowInfo = windowInfo;
  },

  // Display the server certificate (static)
  viewCert() {
    var cert = security._cert;
    viewCertHelper(window, cert);
  },

  _getSecurityInfo() {
    const nsISSLStatusProvider = Components.interfaces.nsISSLStatusProvider;
    const nsISSLStatus = Components.interfaces.nsISSLStatus;

    // We don't have separate info for a frame, return null until further notice
    // (see bug 138479)
    if (!this.windowInfo.isTopWindow)
      return null;

    var hostName = this.windowInfo.hostName;

    var ui = security._getSecurityUI();
    if (!ui)
      return null;

    var isBroken =
      (ui.state & Components.interfaces.nsIWebProgressListener.STATE_IS_BROKEN);
    var isMixed =
      (ui.state & (Components.interfaces.nsIWebProgressListener.STATE_LOADED_MIXED_ACTIVE_CONTENT |
                   Components.interfaces.nsIWebProgressListener.STATE_LOADED_MIXED_DISPLAY_CONTENT));
    var isInsecure =
      (ui.state & Components.interfaces.nsIWebProgressListener.STATE_IS_INSECURE);
    var isEV =
      (ui.state & Components.interfaces.nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL);
    ui.QueryInterface(nsISSLStatusProvider);
    var status = ui.SSLStatus;

    if (!isInsecure && status) {
      status.QueryInterface(nsISSLStatus);
      var cert = status.serverCert;
      var issuerName =
        this.mapIssuerOrganization(cert.issuerOrganization) || cert.issuerName;

      var retval = {
        hostName,
        cAName: issuerName,
        encryptionAlgorithm: undefined,
        encryptionStrength: undefined,
        version: undefined,
        isBroken,
        isMixed,
        isEV,
        cert,
        certificateTransparency: undefined
      };

      var version;
      try {
        retval.encryptionAlgorithm = status.cipherName;
        retval.encryptionStrength = status.secretKeyLength;
        version = status.protocolVersion;
      } catch (e) {
      }

      switch (version) {
        case nsISSLStatus.SSL_VERSION_3:
          retval.version = "SSL 3";
          break;
        case nsISSLStatus.TLS_VERSION_1:
          retval.version = "TLS 1.0";
          break;
        case nsISSLStatus.TLS_VERSION_1_1:
          retval.version = "TLS 1.1";
          break;
        case nsISSLStatus.TLS_VERSION_1_2:
          retval.version = "TLS 1.2"
          break;
        case nsISSLStatus.TLS_VERSION_1_3:
          retval.version = "TLS 1.3"
          break;
      }

      // Select the status text to display for Certificate Transparency.
      // Since we do not yet enforce the CT Policy on secure connections,
      // we must not complain on policy discompliance (it might be viewed
      // as a security issue by the user).
      switch (status.certificateTransparencyStatus) {
        case nsISSLStatus.CERTIFICATE_TRANSPARENCY_NOT_APPLICABLE:
        case nsISSLStatus.CERTIFICATE_TRANSPARENCY_POLICY_NOT_ENOUGH_SCTS:
        case nsISSLStatus.CERTIFICATE_TRANSPARENCY_POLICY_NOT_DIVERSE_SCTS:
          retval.certificateTransparency = null;
          break;
        case nsISSLStatus.CERTIFICATE_TRANSPARENCY_POLICY_COMPLIANT:
          retval.certificateTransparency = "Compliant";
          break;
      }

      return retval;
    }
    return {
      hostName,
      cAName: "",
      encryptionAlgorithm: "",
      encryptionStrength: 0,
      version: "",
      isBroken,
      isMixed,
      isEV,
      cert: null,
      certificateTransparency: null
    };
  },

  // Find the secureBrowserUI object (if present)
  _getSecurityUI() {
    if (window.opener.gBrowser)
      return window.opener.gBrowser.securityUI;
    return null;
  },

  // Interface for mapping a certificate issuer organization to
  // the value to be displayed.
  // Bug 82017 - this implementation should be moved to pipnss C++ code
  mapIssuerOrganization(name) {
    if (!name) return null;

    if (name == "RSA Data Security, Inc.") return "Verisign, Inc.";

    // No mapping required
    return name;
  },

  /**
   * Open the cookie manager window
   */
  viewCookies() {
    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Components.interfaces.nsIWindowMediator);
    var win = wm.getMostRecentWindow("Browser:Cookies");
    var eTLDService = Components.classes["@mozilla.org/network/effective-tld-service;1"].
                      getService(Components.interfaces.nsIEffectiveTLDService);

    var eTLD;
    try {
      eTLD = eTLDService.getBaseDomain(this.uri);
    } catch (e) {
      // getBaseDomain will fail if the host is an IP address or is empty
      eTLD = this.uri.asciiHost;
    }

    if (win) {
      win.gCookiesWindow.setFilter(eTLD);
      win.focus();
    } else
      window.openDialog("chrome://browser/content/preferences/cookies.xul",
                        "Browser:Cookies", "", {filterString: eTLD});
  },

  /**
   * Open the login manager window
   */
  viewPasswords() {
    LoginHelper.openPasswordManager(window, this._getSecurityInfo().hostName);
  },

  _cert: null
};

function securityOnLoad(uri, windowInfo) {
  security.init(uri, windowInfo);

  var info = security._getSecurityInfo();
  if (!info) {
    document.getElementById("securityTab").hidden = true;
    return;
  }
  document.getElementById("securityTab").hidden = false;

  const pageInfoBundle = document.getElementById("pageinfobundle");

  /* Set Identity section text */
  setText("security-identity-domain-value", info.hostName);

  var owner, verifier, validity;
  if (info.cert && !info.isBroken) {
    validity = info.cert.validity.notAfterLocalDay;

    // Try to pull out meaningful values.  Technically these fields are optional
    // so we'll employ fallbacks where appropriate.  The EV spec states that Org
    // fields must be specified for subject and issuer so that case is simpler.
    if (info.isEV) {
      owner = info.cert.organization;
      verifier = security.mapIssuerOrganization(info.cAName);
    } else {
      // Technically, a non-EV cert might specify an owner in the O field or not,
      // depending on the CA's issuing policies.  However we don't have any programmatic
      // way to tell those apart, and no policy way to establish which organization
      // vetting standards are good enough (that's what EV is for) so we default to
      // treating these certs as domain-validated only.
      owner = pageInfoBundle.getString("securityNoOwner");
      verifier = security.mapIssuerOrganization(info.cAName ||
                                                info.cert.issuerCommonName ||
                                                info.cert.issuerName);
    }
  } else {
    // We don't have valid identity credentials.
    owner = pageInfoBundle.getString("securityNoOwner");
    verifier = pageInfoBundle.getString("notset");
  }

  setText("security-identity-owner-value", owner);
  setText("security-identity-verifier-value", verifier);
  if (validity) {
    setText("security-identity-validity-value", validity);
  } else {
    document.getElementById("security-identity-validity-row").hidden = true;
  }

  /* Manage the View Cert button*/
  var viewCert = document.getElementById("security-view-cert");
  if (info.cert) {
    security._cert = info.cert;
    viewCert.collapsed = false;
  } else
    viewCert.collapsed = true;

  /* Set Privacy & History section text */
  var yesStr = pageInfoBundle.getString("yes");
  var noStr = pageInfoBundle.getString("no");

  setText("security-privacy-cookies-value",
          hostHasCookies(uri) ? yesStr : noStr);
  setText("security-privacy-passwords-value",
          realmHasPasswords(uri) ? yesStr : noStr);

  var visitCount = previousVisitCount(info.hostName);
  if (visitCount > 1) {
    setText("security-privacy-history-value",
            pageInfoBundle.getFormattedString("securityNVisits", [visitCount.toLocaleString()]));
  } else if (visitCount == 1) {
    setText("security-privacy-history-value",
            pageInfoBundle.getString("securityOneVisit"));
  } else {
    setText("security-privacy-history-value", noStr);
  }

  /* Set the Technical Detail section messages */
  const pkiBundle = document.getElementById("pkiBundle");
  var hdr;
  var msg1;
  var msg2;

  if (info.isBroken) {
    if (info.isMixed) {
      hdr = pkiBundle.getString("pageInfo_MixedContent");
      msg1 = pkiBundle.getString("pageInfo_MixedContent2");
    } else {
      hdr = pkiBundle.getFormattedString("pageInfo_BrokenEncryption",
                                         [info.encryptionAlgorithm,
                                          info.encryptionStrength + "",
                                          info.version]);
      msg1 = pkiBundle.getString("pageInfo_WeakCipher");
    }
    msg2 = pkiBundle.getString("pageInfo_Privacy_None2");
  } else if (info.encryptionStrength > 0) {
    hdr = pkiBundle.getFormattedString("pageInfo_EncryptionWithBitsAndProtocol",
                                       [info.encryptionAlgorithm,
                                        info.encryptionStrength + "",
                                        info.version]);
    msg1 = pkiBundle.getString("pageInfo_Privacy_Encrypted1");
    msg2 = pkiBundle.getString("pageInfo_Privacy_Encrypted2");
    security._cert = info.cert;
  } else {
    hdr = pkiBundle.getString("pageInfo_NoEncryption");
    if (info.hostName != null)
      msg1 = pkiBundle.getFormattedString("pageInfo_Privacy_None1", [info.hostName]);
    else
      msg1 = pkiBundle.getString("pageInfo_Privacy_None4");
    msg2 = pkiBundle.getString("pageInfo_Privacy_None2");
  }
  setText("security-technical-shortform", hdr);
  setText("security-technical-longform1", msg1);
  setText("security-technical-longform2", msg2);

  const ctStatus =
    document.getElementById("security-technical-certificate-transparency");
  if (info.certificateTransparency) {
    ctStatus.hidden = false;
    ctStatus.value = pkiBundle.getString(
      "pageInfo_CertificateTransparency_" + info.certificateTransparency);
  } else {
    ctStatus.hidden = true;
  }
}

function setText(id, value) {
  var element = document.getElementById(id);
  if (!element)
    return;
  if (element.localName == "textbox" || element.localName == "label")
    element.value = value;
  else {
    if (element.hasChildNodes())
      element.firstChild.remove();
    var textNode = document.createTextNode(value);
    element.appendChild(textNode);
  }
}

function viewCertHelper(parent, cert) {
  if (!cert)
    return;

  var cd = Components.classes[CERTIFICATEDIALOGS_CONTRACTID].getService(nsICertificateDialogs);
  cd.viewCert(parent, cert);
}

/**
 * Return true iff we have cookies for uri
 */
function hostHasCookies(uri) {
  var cookieManager = Components.classes["@mozilla.org/cookiemanager;1"]
                                .getService(Components.interfaces.nsICookieManager2);

  return cookieManager.countCookiesFromHost(uri.asciiHost) > 0;
}

/**
 * Return true iff realm (proto://host:port) (extracted from uri) has
 * saved passwords
 */
function realmHasPasswords(uri) {
  var passwordManager = Components.classes["@mozilla.org/login-manager;1"]
                                  .getService(Components.interfaces.nsILoginManager);
  return passwordManager.countLogins(uri.prePath, "", "") > 0;
}

/**
 * Return the number of previous visits recorded for host before today.
 *
 * @param host - the domain name to look for in history
 */
function previousVisitCount(host, endTimeReference) {
  if (!host)
    return false;

  var historyService = Components.classes["@mozilla.org/browser/nav-history-service;1"]
                                 .getService(Components.interfaces.nsINavHistoryService);

  var options = historyService.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_VISIT;

  // Search for visits to this host before today
  var query = historyService.getNewQuery();
  query.endTimeReference = query.TIME_RELATIVE_TODAY;
  query.endTime = 0;
  query.domain = host;

  var result = historyService.executeQuery(query, options);
  result.root.containerOpen = true;
  var cc = result.root.childCount;
  result.root.containerOpen = false;
  return cc;
}
