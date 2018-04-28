/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This content script should work in any browser or iframe and should not
 * depend on the frame being contained in tabbrowser. */

/* eslint-env mozilla/frame-script */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  ContentLinkHandler: "resource:///modules/ContentLinkHandler.jsm",
  ContentMetaHandler: "resource:///modules/ContentMetaHandler.jsm",
  ContentWebRTC: "resource:///modules/ContentWebRTC.jsm",
  InlineSpellCheckerContent: "resource://gre/modules/InlineSpellCheckerContent.jsm",
  LoginManagerContent: "resource://gre/modules/LoginManagerContent.jsm",
  LoginFormFactory: "resource://gre/modules/LoginManagerContent.jsm",
  InsecurePasswordUtils: "resource://gre/modules/InsecurePasswordUtils.jsm",
  PluginContent: "resource:///modules/PluginContent.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  FormSubmitObserver: "resource:///modules/FormSubmitObserver.jsm",
  PageMetadata: "resource://gre/modules/PageMetadata.jsm",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.jsm",
  SafeBrowsing: "resource://gre/modules/SafeBrowsing.jsm",
  Utils: "resource://gre/modules/sessionstore/Utils.jsm",
  WebNavigationFrames: "resource://gre/modules/WebNavigationFrames.jsm",
  Feeds: "resource:///modules/Feeds.jsm",
  ContextMenu: "resource:///modules/ContextMenu.jsm",
});

XPCOMUtils.defineLazyGetter(this, "gPipNSSBundle", function() {
  return Services.strings.createBundle("chrome://pipnss/locale/pipnss.properties");
});
XPCOMUtils.defineLazyGetter(this, "gNSSErrorsBundle", function() {
  return Services.strings.createBundle("chrome://pipnss/locale/nsserrors.properties");
});

// TabChildGlobal
var global = this;

var contextMenu = this.contextMenu = new ContextMenu(global);

// Load the form validation popup handler
var formSubmitObserver = new FormSubmitObserver(content, this);

addMessageListener("RemoteLogins:fillForm", function(message) {
  // intercept if ContextMenu.jsm had sent a plain object for remote targets
  message.objects.inputElement = contextMenu.getTarget(message, "inputElement");
  LoginManagerContent.receiveMessage(message, content);
});
addEventListener("DOMFormHasPassword", function(event) {
  LoginManagerContent.onDOMFormHasPassword(event, content);
  let formLike = LoginFormFactory.createFromForm(event.originalTarget);
  InsecurePasswordUtils.reportInsecurePasswords(formLike);
});
addEventListener("DOMInputPasswordAdded", function(event) {
  LoginManagerContent.onDOMInputPasswordAdded(event, content);
  let formLike = LoginFormFactory.createFromField(event.originalTarget);
  InsecurePasswordUtils.reportInsecurePasswords(formLike);
});
addEventListener("pageshow", function(event) {
  LoginManagerContent.onPageShow(event, content);
});
addEventListener("DOMAutoComplete", function(event) {
  LoginManagerContent.onUsernameInput(event);
});
addEventListener("blur", function(event) {
  LoginManagerContent.onUsernameInput(event);
});

const SEC_ERROR_BASE          = Ci.nsINSSErrorsService.NSS_SEC_ERROR_BASE;
const MOZILLA_PKIX_ERROR_BASE = Ci.nsINSSErrorsService.MOZILLA_PKIX_ERROR_BASE;

const SEC_ERROR_EXPIRED_CERTIFICATE                = SEC_ERROR_BASE + 11;
const SEC_ERROR_UNKNOWN_ISSUER                     = SEC_ERROR_BASE + 13;
const SEC_ERROR_UNTRUSTED_ISSUER                   = SEC_ERROR_BASE + 20;
const SEC_ERROR_UNTRUSTED_CERT                     = SEC_ERROR_BASE + 21;
const SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE         = SEC_ERROR_BASE + 30;
const SEC_ERROR_CA_CERT_INVALID                    = SEC_ERROR_BASE + 36;
const SEC_ERROR_OCSP_FUTURE_RESPONSE               = SEC_ERROR_BASE + 131;
const SEC_ERROR_OCSP_OLD_RESPONSE                  = SEC_ERROR_BASE + 132;
const SEC_ERROR_REUSED_ISSUER_AND_SERIAL           = SEC_ERROR_BASE + 138;
const SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED  = SEC_ERROR_BASE + 176;
const MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE = MOZILLA_PKIX_ERROR_BASE + 5;
const MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE = MOZILLA_PKIX_ERROR_BASE + 6;
const MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT          = MOZILLA_PKIX_ERROR_BASE + 14;
const MOZILLA_PKIX_ERROR_MITM_DETECTED             = MOZILLA_PKIX_ERROR_BASE + 15;


const SSL_ERROR_BASE = Ci.nsINSSErrorsService.NSS_SSL_ERROR_BASE;
const SSL_ERROR_SSL_DISABLED  = SSL_ERROR_BASE + 20;
const SSL_ERROR_SSL2_DISABLED  = SSL_ERROR_BASE + 14;

const PREF_SERVICES_SETTINGS_CLOCK_SKEW_SECONDS = "services.settings.clock_skew_seconds";
const PREF_SERVICES_SETTINGS_LAST_FETCHED       = "services.settings.last_update_seconds";

const PREF_SSL_IMPACT_ROOTS = ["security.tls.version.", "security.ssl3."];


function getSerializedSecurityInfo(docShell) {
  let serhelper = Cc["@mozilla.org/network/serialization-helper;1"]
                    .getService(Ci.nsISerializationHelper);

  let securityInfo = docShell.failedChannel && docShell.failedChannel.securityInfo;
  if (!securityInfo) {
    return "";
  }
  securityInfo.QueryInterface(Ci.nsITransportSecurityInfo)
              .QueryInterface(Ci.nsISerializable);

  return serhelper.serializeToString(securityInfo);
}

function getSiteBlockedErrorDetails(docShell) {
  let blockedInfo = {};
  if (docShell.failedChannel) {
    let classifiedChannel = docShell.failedChannel.
                            QueryInterface(Ci.nsIClassifiedChannel);
    if (classifiedChannel) {
      let httpChannel = docShell.failedChannel.QueryInterface(Ci.nsIHttpChannel);

      let reportUri = httpChannel.URI.clone();

      // Remove the query to avoid leaking sensitive data
      if (reportUri instanceof Ci.nsIURL) {
        reportUri = reportUri.mutate()
                             .setQuery("")
                             .finalize();
      }

      blockedInfo = { list: classifiedChannel.matchedList,
                      provider: classifiedChannel.matchedProvider,
                      uri: reportUri.asciiSpec };
    }
  }
  return blockedInfo;
}

var AboutBlockedSiteListener = {
  init(chromeGlobal) {
    addMessageListener("DeceptiveBlockedDetails", this);
    chromeGlobal.addEventListener("AboutBlockedLoaded", this, false, true);
  },

  get isBlockedSite() {
    return content.document.documentURI.startsWith("about:blocked");
  },

  receiveMessage(msg) {
    if (!this.isBlockedSite) {
      return;
    }

    if (msg.name == "DeceptiveBlockedDetails") {
      sendAsyncMessage("DeceptiveBlockedDetails:Result", {
        blockedInfo: getSiteBlockedErrorDetails(docShell),
      });
    }
  },

  handleEvent(aEvent) {
    if (!this.isBlockedSite) {
      return;
    }

    if (aEvent.type != "AboutBlockedLoaded") {
      return;
    }

    let blockedInfo = getSiteBlockedErrorDetails(docShell);
    let provider = blockedInfo.provider || "";

    let doc = content.document;

    /**
    * Set error description link in error details.
    * For example, the "reported as a deceptive site" link for
    * blocked phishing pages.
    */
    let desc = Services.prefs.getCharPref(
      "browser.safebrowsing.provider." + provider + ".reportURL", "");
    if (desc) {
      doc.getElementById("error_desc_link").setAttribute("href", desc + aEvent.detail.url);
    }

    // Set other links in error details.
    switch (aEvent.detail.err) {
      case "malware":
        doc.getElementById("report_detection").setAttribute("href",
          (SafeBrowsing.getReportURL("MalwareMistake", blockedInfo) ||
           "https://www.stopbadware.org/firefox"));
        doc.getElementById("learn_more_link").setAttribute("href",
          "https://www.stopbadware.org/firefox");
        break;
      case "unwanted":
        doc.getElementById("learn_more_link").setAttribute("href",
          "https://www.google.com/about/unwanted-software-policy.html");
        break;
      case "phishing":
        doc.getElementById("report_detection").setAttribute("href",
          (SafeBrowsing.getReportURL("PhishMistake", blockedInfo) ||
           "https://safebrowsing.google.com/safebrowsing/report_error/?tpl=mozilla"));
        doc.getElementById("learn_more_link").setAttribute("href",
          "https://www.antiphishing.org//");
        break;
    }

    // Set the firefox support url.
    doc.getElementById("firefox_support").setAttribute("href",
      Services.urlFormatter.formatURLPref("app.support.baseURL") + "phishing-malware");

    // Show safe browsing details on load if the pref is set to true.
    let showDetails = Services.prefs.getBoolPref("browser.xul.error_pages.show_safe_browsing_details_on_load");
    if (showDetails) {
      let details = content.document.getElementById("errorDescriptionContainer");
      details.removeAttribute("hidden");
    }

    // Set safe browsing advisory link.
    let advisoryUrl = Services.prefs.getCharPref(
      "browser.safebrowsing.provider." + provider + ".advisoryURL", "");
    if (!advisoryUrl) {
      let el = content.document.getElementById("advisoryDesc");
      el.remove();
      return;
    }

    let advisoryLinkText = Services.prefs.getCharPref(
      "browser.safebrowsing.provider." + provider + ".advisoryName", "");
    if (!advisoryLinkText) {
      let el = content.document.getElementById("advisoryDesc");
      el.remove();
      return;
    }

    let anchorEl = content.document.getElementById("advisory_provider");
    anchorEl.setAttribute("href", advisoryUrl);
    anchorEl.textContent = advisoryLinkText;
  },
};

var AboutNetAndCertErrorListener = {
  init(chromeGlobal) {
    addMessageListener("CertErrorDetails", this);
    addMessageListener("Browser:CaptivePortalFreed", this);
    chromeGlobal.addEventListener("AboutNetErrorLoad", this, false, true);
    chromeGlobal.addEventListener("AboutNetErrorOpenCaptivePortal", this, false, true);
    chromeGlobal.addEventListener("AboutNetErrorSetAutomatic", this, false, true);
    chromeGlobal.addEventListener("AboutNetErrorResetPreferences", this, false, true);
  },

  isAboutNetError(doc) {
    return doc.documentURI.startsWith("about:neterror");
  },

  isAboutCertError(doc) {
    return doc.documentURI.startsWith("about:certerror");
  },

  receiveMessage(msg) {
    if (msg.name == "CertErrorDetails") {
      let frameDocShell = WebNavigationFrames.findDocShell(msg.data.frameId, docShell);
      // We need nsIWebNavigation to access docShell.document.
      frameDocShell && frameDocShell.QueryInterface(Ci.nsIWebNavigation);
      if (!frameDocShell || !this.isAboutCertError(frameDocShell.document)) {
        return;
      }

      this.onCertErrorDetails(msg, frameDocShell);
    } else if (msg.name == "Browser:CaptivePortalFreed") {
      // TODO: This check is not correct for frames.
      if (!this.isAboutCertError(content.document)) {
        return;
      }

      this.onCaptivePortalFreed(msg);
    }
  },

  _getCertValidityRange(docShell) {
    let {securityInfo} = docShell.failedChannel;
    securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
    let certs = securityInfo.failedCertChain.getEnumerator();
    let notBefore = 0;
    let notAfter = Number.MAX_SAFE_INTEGER;
    while (certs.hasMoreElements()) {
      let cert = certs.getNext();
      cert.QueryInterface(Ci.nsIX509Cert);
      notBefore = Math.max(notBefore, cert.validity.notBefore);
      notAfter = Math.min(notAfter, cert.validity.notAfter);
    }
    // nsIX509Cert reports in PR_Date terms, which uses microseconds. Convert:
    notBefore /= 1000;
    notAfter /= 1000;
    return {notBefore, notAfter};
  },

  _setTechDetails(input, doc) {
    // CSS class and error code are set from nsDocShell.
    let searchParams = new URLSearchParams(doc.documentURI.split("?")[1]);
    let cssClass = searchParams.get("s");
    let error = searchParams.get("e");
    let technicalInfo = doc.getElementById("badCertTechnicalInfo");
    technicalInfo.textContent = "";

    let uri = Services.io.newURI(input.data.url);
    let hostString = uri.host;
    if (uri.port != 443 && uri.port != -1) {
      hostString = uri.hostPort;
    }

    let msg1 = gPipNSSBundle.formatStringFromName("certErrorIntro",
                                                  [hostString], 1);
    msg1 += "\n\n";

    if (input.data.certIsUntrusted) {
      switch (input.data.code) {
        // We only want to measure MitM rates for now. Treat it as unkown issuer.
        case MOZILLA_PKIX_ERROR_MITM_DETECTED:
        case SEC_ERROR_UNKNOWN_ISSUER:
          msg1 += gPipNSSBundle.GetStringFromName("certErrorTrust_UnknownIssuer") + "\n";
          msg1 += gPipNSSBundle.GetStringFromName("certErrorTrust_UnknownIssuer2") + "\n";
          msg1 += gPipNSSBundle.GetStringFromName("certErrorTrust_UnknownIssuer3") + "\n";
          break;
        case SEC_ERROR_CA_CERT_INVALID:
          msg1 += gPipNSSBundle.GetStringFromName("certErrorTrust_CaInvalid") + "\n";
          break;
        case SEC_ERROR_UNTRUSTED_ISSUER:
          msg1 += gPipNSSBundle.GetStringFromName("certErrorTrust_Issuer") + "\n";
          break;
        case SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED:
          msg1 += gPipNSSBundle.GetStringFromName("certErrorTrust_SignatureAlgorithmDisabled") + "\n";
          break;
        case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
          msg1 += gPipNSSBundle.GetStringFromName("certErrorTrust_ExpiredIssuer") + "\n";
          break;
        case MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT:
          msg1 += gPipNSSBundle.GetStringFromName("certErrorTrust_SelfSigned") + "\n";
          break;
        default:
          msg1 += gPipNSSBundle.GetStringFromName("certErrorTrust_Untrusted") + "\n";
      }
    }

    technicalInfo.appendChild(doc.createTextNode(msg1));

    if (input.data.isDomainMismatch) {
      let subjectAltNames = input.data.certSubjectAltNames.split(",");
      let numSubjectAltNames = subjectAltNames.length;
      let msgPrefix = "";
      if (numSubjectAltNames != 0) {
        if (numSubjectAltNames == 1) {
          msgPrefix = gPipNSSBundle.GetStringFromName("certErrorMismatchSinglePrefix");

          // Let's check if we want to make this a link.
          let okHost = input.data.certSubjectAltNames;
          let href = "";
          let thisHost = doc.location.hostname;
          let proto = doc.location.protocol + "//";
          // If okHost is a wildcard domain ("*.example.com") let's
          // use "www" instead.  "*.example.com" isn't going to
          // get anyone anywhere useful. bug 432491
          okHost = okHost.replace(/^\*\./, "www.");
          /* case #1:
           * example.com uses an invalid security certificate.
           *
           * The certificate is only valid for www.example.com
           *
           * Make sure to include the "." ahead of thisHost so that
           * a MitM attack on paypal.com doesn't hyperlink to "notpaypal.com"
           *
           * We'd normally just use a RegExp here except that we lack a
           * library function to escape them properly (bug 248062), and
           * domain names are famous for having '.' characters in them,
           * which would allow spurious and possibly hostile matches.
           */
          if (okHost.endsWith("." + thisHost)) {
            href = proto + okHost;
          }
          /* case #2:
           * browser.garage.maemo.org uses an invalid security certificate.
           *
           * The certificate is only valid for garage.maemo.org
           */
          if (thisHost.endsWith("." + okHost)) {
            href = proto + okHost;
          }

          // If we set a link, meaning there's something helpful for
          // the user here, expand the section by default
          if (href && cssClass != "expertBadCert") {
            doc.getElementById("badCertAdvancedPanel").style.display = "block";
            if (error == "nssBadCert") {
              // Toggling the advanced panel must ensure that the debugging
              // information panel is hidden as well, since it's opened by the
              // error code link in the advanced panel.
              var div = doc.getElementById("certificateErrorDebugInformation");
              div.style.display = "none";
            }
          }

          // Set the link if we want it.
          if (href) {
            let referrerlink = doc.createElement("a");
            referrerlink.append(input.data.certSubjectAltNames);
            referrerlink.title = input.data.certSubjectAltNames;
            referrerlink.id = "cert_domain_link";
            referrerlink.href = href;
            let fragment = BrowserUtils.getLocalizedFragment(doc, msgPrefix,
                                                             referrerlink);
            technicalInfo.appendChild(fragment);
          } else {
            let fragment = BrowserUtils.getLocalizedFragment(doc,
                                                             msgPrefix,
                                                             input.data.certSubjectAltNames);
            technicalInfo.appendChild(fragment);
          }
          technicalInfo.append("\n");
        } else {
          let msg = gPipNSSBundle.GetStringFromName("certErrorMismatchMultiple") + "\n";
          for (let i = 0; i < numSubjectAltNames; i++) {
            msg += subjectAltNames[i];
            if (i != (numSubjectAltNames - 1)) {
              msg += ", ";
            }
          }
          technicalInfo.append(msg + "\n");
        }
      } else {
        let msg = gPipNSSBundle.formatStringFromName("certErrorMismatch",
                                                     [hostString], 1);
        technicalInfo.append(msg + "\n");
      }
    }

    if (input.data.isNotValidAtThisTime) {
      let nowTime = new Date().getTime() * 1000;
      let dateOptions = { year: "numeric", month: "long", day: "numeric", hour: "numeric", minute: "numeric" };
      let now = new Services.intl.DateTimeFormat(undefined, dateOptions).format(new Date());
      let msg = "";
      if (input.data.validity.notBefore) {
        if (nowTime > input.data.validity.notAfter) {
          msg += gPipNSSBundle.formatStringFromName("certErrorExpiredNow",
                                                    [input.data.validity.notAfterLocalTime, now], 2) + "\n";
        } else {
          msg += gPipNSSBundle.formatStringFromName("certErrorNotYetValidNow",
                                                    [input.data.validity.notBeforeLocalTime, now], 2) + "\n";
        }
      } else {
        // If something goes wrong, we assume the cert expired.
        msg += gPipNSSBundle.formatStringFromName("certErrorExpiredNow",
                                                  ["", now], 2) + "\n";
      }
      technicalInfo.append(msg);
    }
    technicalInfo.append("\n");

    // Add link to certificate and error message.
    let linkPrefix = gPipNSSBundle.GetStringFromName("certErrorCodePrefix3");
    let detailLink = doc.createElement("a");
    detailLink.append(input.data.codeString);
    detailLink.title = input.data.codeString;
    detailLink.id = "errorCode";
    let fragment = BrowserUtils.getLocalizedFragment(doc, linkPrefix, detailLink);
    technicalInfo.appendChild(fragment);
    var errorCode = doc.getElementById("errorCode");
    if (errorCode) {
      errorCode.href = "javascript:void(0)";
      errorCode.addEventListener("click", () => {
        let debugInfo = doc.getElementById("certificateErrorDebugInformation");
        debugInfo.style.display = "block";
        debugInfo.scrollIntoView({block: "start", behavior: "smooth"});
      });
    }
  },

  onCertErrorDetails(msg, docShell) {
    let doc = docShell.document;

    let div = doc.getElementById("certificateErrorText");
    div.textContent = msg.data.info;
    this._setTechDetails(msg, doc);
    let learnMoreLink = doc.getElementById("learnMoreLink");
    let baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");

    switch (msg.data.code) {
      case SEC_ERROR_UNKNOWN_ISSUER:
      case MOZILLA_PKIX_ERROR_MITM_DETECTED:
      case MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT:
        learnMoreLink.href = baseURL + "security-error";
        break;

      // In case the certificate expired we make sure the system clock
      // matches the remote-settings service (blocklist via Kinto) ping time
      // and is not before the build date.
      case SEC_ERROR_EXPIRED_CERTIFICATE:
      case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
      case SEC_ERROR_OCSP_FUTURE_RESPONSE:
      case SEC_ERROR_OCSP_OLD_RESPONSE:
      case MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE:
      case MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE:

        // We check against the remote-settings server time first if available, because that allows us
        // to give the user an approximation of what the correct time is.
        let difference = Services.prefs.getIntPref(PREF_SERVICES_SETTINGS_CLOCK_SKEW_SECONDS, 0);
        let lastFetched = Services.prefs.getIntPref(PREF_SERVICES_SETTINGS_LAST_FETCHED, 0) * 1000;

        let now = Date.now();
        let certRange = this._getCertValidityRange(docShell);

        let approximateDate = now - difference * 1000;
        // If the difference is more than a day, we last fetched the date in the last 5 days,
        // and adjusting the date per the interval would make the cert valid, warn the user:
        if (Math.abs(difference) > 60 * 60 * 24 && (now - lastFetched) <= 60 * 60 * 24 * 5 &&
            certRange.notBefore < approximateDate && certRange.notAfter > approximateDate) {
          let formatter = new Services.intl.DateTimeFormat(undefined, {
            dateStyle: "short"
          });
          let systemDate = formatter.format(new Date());
          // negative difference means local time is behind server time
          approximateDate = formatter.format(new Date(approximateDate));

          doc.getElementById("wrongSystemTime_URL").textContent = doc.location.hostname;
          doc.getElementById("wrongSystemTime_systemDate").textContent = systemDate;
          doc.getElementById("wrongSystemTime_actualDate").textContent = approximateDate;

          doc.getElementById("errorShortDesc").style.display = "none";
          doc.getElementById("wrongSystemTimePanel").style.display = "block";

        // If there is no clock skew with Kinto servers, check against the build date.
        // (The Kinto ping could have happened when the time was still right, or not at all)
        } else {
          let appBuildID = Services.appinfo.appBuildID;

          let year = parseInt(appBuildID.substr(0, 4), 10);
          let month = parseInt(appBuildID.substr(4, 2), 10) - 1;
          let day = parseInt(appBuildID.substr(6, 2), 10);

          let buildDate = new Date(year, month, day);
          let systemDate = new Date();

          // We don't check the notBefore of the cert with the build date,
          // as it is of course almost certain that it is now later than the build date,
          // so we shouldn't exclude the possibility that the cert has become valid
          // since the build date.
          if (buildDate > systemDate && new Date(certRange.notAfter) > buildDate) {
            let formatter = new Services.intl.DateTimeFormat(undefined, {
              dateStyle: "short"
            });

            doc.getElementById("wrongSystemTimeWithoutReference_URL")
              .textContent = doc.location.hostname;
            doc.getElementById("wrongSystemTimeWithoutReference_systemDate")
              .textContent = formatter.format(systemDate);

            doc.getElementById("errorShortDesc").style.display = "none";
            doc.getElementById("wrongSystemTimeWithoutReferencePanel").style.display = "block";
          }
        }
        learnMoreLink.href = baseURL + "time-errors";
        break;
    }
  },

  onCaptivePortalFreed(msg) {
    content.dispatchEvent(new content.CustomEvent("AboutNetErrorCaptivePortalFreed"));
  },

  handleEvent(aEvent) {
    let doc;
    if (aEvent.originalTarget instanceof Ci.nsIDOMDocument) {
      doc = aEvent.originalTarget;
    } else {
      doc = aEvent.originalTarget.ownerDocument;
    }

    if (!this.isAboutNetError(doc) && !this.isAboutCertError(doc)) {
      return;
    }

    switch (aEvent.type) {
    case "AboutNetErrorLoad":
      this.onPageLoad(aEvent.originalTarget, doc.defaultView);
      break;
    case "AboutNetErrorOpenCaptivePortal":
      this.openCaptivePortalPage(aEvent);
      break;
    case "AboutNetErrorSetAutomatic":
      this.onSetAutomatic(aEvent);
      break;
    case "AboutNetErrorResetPreferences":
      this.onResetPreferences(aEvent);
      break;
    }
  },

  changedCertPrefs() {
    let prefSSLImpact = PREF_SSL_IMPACT_ROOTS.reduce((prefs, root) => {
       return prefs.concat(Services.prefs.getChildList(root));
    }, []);
    for (let prefName of prefSSLImpact) {
      if (Services.prefs.prefHasUserValue(prefName)) {
        return true;
      }
    }

    return false;
  },

   _getErrorMessageFromCode(securityInfo, doc) {
     let uri = Services.io.newURI(doc.location);
     let hostString = uri.host;
     if (uri.port != 443 && uri.port != -1) {
       hostString = uri.hostPort;
     }

     let id_str = "";
     switch (securityInfo.errorCode) {
       case SSL_ERROR_SSL_DISABLED:
         id_str = "PSMERR_SSL_Disabled";
         break;
       case SSL_ERROR_SSL2_DISABLED:
         id_str = "PSMERR_SSL2_Disabled";
         break;
       case SEC_ERROR_REUSED_ISSUER_AND_SERIAL:
         id_str = "PSMERR_HostReusedIssuerSerial";
         break;
     }
     let nss_error_id_str = securityInfo.errorCodeString;
     let msg2 = "";
     if (id_str) {
       msg2 = gPipNSSBundle.GetStringFromName(id_str) + "\n";
     } else if (nss_error_id_str) {
       msg2 = gNSSErrorsBundle.GetStringFromName(nss_error_id_str) + "\n";
     }

     if (!msg2) {
       // We couldn't get an error message. Use the error string.
       // Note that this is different from before where we used PR_ErrorToString.
       msg2 = nss_error_id_str;
     }
     let msg = gPipNSSBundle.formatStringFromName("SSLConnectionErrorPrefix2",
                                                  [hostString, msg2], 2);

     if (nss_error_id_str) {
       msg += gPipNSSBundle.formatStringFromName("certErrorCodePrefix3",
                                                 [nss_error_id_str], 1) + "\n";
     }
     return msg;
   },

  onPageLoad(originalTarget, win) {
    // Values for telemtery bins: see TLS_ERROR_REPORT_UI in Histograms.json
    const TLS_ERROR_REPORT_TELEMETRY_UI_SHOWN = 0;

    let hideAddExceptionButton = false;

    if (this.isAboutCertError(win.document)) {
      ClickEventHandler.onCertError(originalTarget, win);
      hideAddExceptionButton =
        Services.prefs.getBoolPref("security.certerror.hideAddException", false);
    }
    if (this.isAboutNetError(win.document)) {
      let docShell = win.document.docShell;
      if (docShell) {
        let {securityInfo} = docShell.failedChannel;
        // We don't have a securityInfo when this is for example a DNS error.
        if (securityInfo) {
          securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
          let msg = this._getErrorMessageFromCode(securityInfo,
                                                  win.document);
          let id = win.document.getElementById("errorShortDescText");
          id.textContent = msg;
        }
      }
    }

    let automatic = Services.prefs.getBoolPref("security.ssl.errorReporting.automatic");
    win.dispatchEvent(new win.CustomEvent("AboutNetErrorOptions", {
      detail: JSON.stringify({
        enabled: Services.prefs.getBoolPref("security.ssl.errorReporting.enabled"),
        changedCertPrefs: this.changedCertPrefs(),
        automatic,
        hideAddExceptionButton,
      })
    }));

    sendAsyncMessage("Browser:SSLErrorReportTelemetry",
                     {reportStatus: TLS_ERROR_REPORT_TELEMETRY_UI_SHOWN});
  },

  openCaptivePortalPage(evt) {
    sendAsyncMessage("Browser:OpenCaptivePortalPage");
  },


  onResetPreferences(evt) {
    sendAsyncMessage("Browser:ResetSSLPreferences");
  },

  onSetAutomatic(evt) {
    sendAsyncMessage("Browser:SetSSLErrorReportAuto", {
      automatic: evt.detail
    });

    // If we're enabling reports, send a report for this failure.
    if (evt.detail) {
      let win = evt.originalTarget.ownerGlobal;
      let docShell = win.document.docShell;

      let {securityInfo} = docShell.failedChannel;
      securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
      let {host, port} = win.document.mozDocumentURIIfNotForErrorPages;

      let errorReporter = Cc["@mozilla.org/securityreporter;1"]
                            .getService(Ci.nsISecurityReporter);
      errorReporter.reportTLSError(securityInfo, host, port);
    }
  },
};

AboutNetAndCertErrorListener.init(this);
AboutBlockedSiteListener.init(this);

var ClickEventHandler = {
  init: function init() {
    Services.els.addSystemEventListener(global, "click", this, true);
  },

  handleEvent(event) {
    if (!event.isTrusted || event.defaultPrevented || event.button == 2) {
      return;
    }

    let originalTarget = event.originalTarget;
    let ownerDoc = originalTarget.ownerDocument;
    if (!ownerDoc) {
      return;
    }

    // Handle click events from about pages
    if (event.button == 0) {
      if (AboutNetAndCertErrorListener.isAboutCertError(ownerDoc)) {
        this.onCertError(originalTarget, ownerDoc.defaultView);
        return;
      } else if (ownerDoc.documentURI.startsWith("about:blocked")) {
        this.onAboutBlocked(originalTarget, ownerDoc);
        return;
      } else if (AboutNetAndCertErrorListener.isAboutNetError(ownerDoc)) {
        this.onAboutNetError(event, ownerDoc.documentURI);
        return;
      }
    }

    let [href, node, principal] = this._hrefAndLinkNodeForClickEvent(event);

    // get referrer attribute from clicked link and parse it
    // if per element referrer is enabled, the element referrer overrules
    // the document wide referrer
    let referrerPolicy = ownerDoc.referrerPolicy;
    if (node) {
      let referrerAttrValue = Services.netUtils.parseAttributePolicyString(node.
                              getAttribute("referrerpolicy"));
      if (referrerAttrValue !== Ci.nsIHttpChannel.REFERRER_POLICY_UNSET) {
        referrerPolicy = referrerAttrValue;
      }
    }

    let frameOuterWindowID = WebNavigationFrames.getFrameId(ownerDoc.defaultView);

    let json = { button: event.button, shiftKey: event.shiftKey,
                 ctrlKey: event.ctrlKey, metaKey: event.metaKey,
                 altKey: event.altKey, href: null, title: null,
                 bookmark: false, frameOuterWindowID, referrerPolicy,
                 triggeringPrincipal: principal,
                 originAttributes: principal ? principal.originAttributes : {},
                 isContentWindowPrivate: PrivateBrowsingUtils.isContentWindowPrivate(ownerDoc.defaultView)};

    if (href) {
      try {
        BrowserUtils.urlSecurityCheck(href, principal);
      } catch (e) {
        return;
      }

      json.href = href;
      if (node) {
        json.title = node.getAttribute("title");
        if (event.button == 0 && !event.ctrlKey && !event.shiftKey &&
            !event.altKey && !event.metaKey) {
          json.bookmark = node.getAttribute("rel") == "sidebar";
          if (json.bookmark) {
            event.preventDefault(); // Need to prevent the pageload.
          }
        }
      }
      json.noReferrer = BrowserUtils.linkHasNoReferrer(node);

      // Check if the link needs to be opened with mixed content allowed.
      // Only when the owner doc has |mixedContentChannel| and the same origin
      // should we allow mixed content.
      json.allowMixedContent = false;
      let docshell = ownerDoc.docShell;
      if (docShell.mixedContentChannel) {
        const sm = Services.scriptSecurityManager;
        try {
          let targetURI = Services.io.newURI(href);
          sm.checkSameOriginURI(docshell.mixedContentChannel.URI, targetURI, false);
          json.allowMixedContent = true;
        } catch (e) {}
      }
      json.originPrincipal = ownerDoc.nodePrincipal;
      json.triggeringPrincipal = ownerDoc.nodePrincipal;

      sendAsyncMessage("Content:Click", json);
      return;
    }

    // This might be middle mouse navigation.
    if (event.button == 1) {
      sendAsyncMessage("Content:Click", json);
    }
  },

  onCertError(targetElement, win) {
    let docShell = win.document.docShell;
    sendAsyncMessage("Browser:CertExceptionError", {
      frameId: WebNavigationFrames.getFrameId(win),
      location: win.document.location.href,
      elementId: targetElement.getAttribute("id"),
      isTopFrame: (win.parent === win),
      securityInfoAsString: getSerializedSecurityInfo(docShell),
    });
  },

  onAboutBlocked(targetElement, ownerDoc) {
    var reason = "phishing";
    if (/e=malwareBlocked/.test(ownerDoc.documentURI)) {
      reason = "malware";
    } else if (/e=unwantedBlocked/.test(ownerDoc.documentURI)) {
      reason = "unwanted";
    } else if (/e=harmfulBlocked/.test(ownerDoc.documentURI)) {
      reason = "harmful";
    }

    let docShell = ownerDoc.defaultView.QueryInterface(Ci.nsIInterfaceRequestor)
                                       .getInterface(Ci.nsIWebNavigation)
                                      .QueryInterface(Ci.nsIDocShell);

    sendAsyncMessage("Browser:SiteBlockedError", {
      location: ownerDoc.location.href,
      reason,
      elementId: targetElement.getAttribute("id"),
      isTopFrame: (ownerDoc.defaultView.parent === ownerDoc.defaultView),
      blockedInfo: getSiteBlockedErrorDetails(docShell),
    });
  },

  onAboutNetError(event, documentURI) {
    let elmId = event.originalTarget.getAttribute("id");
    if (elmId == "returnButton") {
      sendAsyncMessage("Browser:SSLErrorGoBack", {});
      return;
    }
    if (elmId != "errorTryAgain" || !/e=netOffline/.test(documentURI)) {
      return;
    }
    // browser front end will handle clearing offline mode and refreshing
    // the page *if* we're in offline mode now. Otherwise let the error page
    // handle the click.
    if (Services.io.offline) {
      event.preventDefault();
      sendAsyncMessage("Browser:EnableOnlineMode", {});
    }
  },

  /**
   * Extracts linkNode and href for the current click target.
   *
   * @param event
   *        The click event.
   * @return [href, linkNode, linkPrincipal].
   *
   * @note linkNode will be null if the click wasn't on an anchor
   *       element. This includes SVG links, because callers expect |node|
   *       to behave like an <a> element, which SVG links (XLink) don't.
   */
  _hrefAndLinkNodeForClickEvent(event) {
    function isHTMLLink(aNode) {
      // Be consistent with what nsContextMenu.js does.
      return ((aNode instanceof content.HTMLAnchorElement && aNode.href) ||
              (aNode instanceof content.HTMLAreaElement && aNode.href) ||
              aNode instanceof content.HTMLLinkElement);
    }

    let node = event.target;
    while (node && !isHTMLLink(node)) {
      node = node.parentNode;
    }

    if (node)
      return [node.href, node, node.ownerDocument.nodePrincipal];

    // If there is no linkNode, try simple XLink.
    let href, baseURI;
    node = event.target;
    while (node && !href) {
      if (node.nodeType == content.Node.ELEMENT_NODE &&
          (node.localName == "a" ||
           node.namespaceURI == "http://www.w3.org/1998/Math/MathML")) {
        href = node.getAttribute("href") ||
               node.getAttributeNS("http://www.w3.org/1999/xlink", "href");
        if (href) {
          baseURI = node.ownerDocument.baseURIObject;
          break;
        }
      }
      node = node.parentNode;
    }

    // In case of XLink, we don't return the node we got href from since
    // callers expect <a>-like elements.
    // Note: makeURI() will throw if aUri is not a valid URI.
    return [href ? Services.io.newURI(href, null, baseURI).spec : null, null,
            node && node.ownerDocument.nodePrincipal];
  }
};
ClickEventHandler.init();

ContentLinkHandler.init(this);
ContentMetaHandler.init(this);

// TODO: Load this lazily so the JSM is run only if a relevant event/message fires.
var pluginContent = new PluginContent(global);

addEventListener("DOMWindowFocus", function(event) {
  sendAsyncMessage("DOMWindowFocus", {});
}, false);

// We use this shim so that ContentWebRTC.jsm will not be loaded until
// it is actually needed.
var ContentWebRTCShim = message => ContentWebRTC.receiveMessage(message);

addMessageListener("rtcpeer:Allow", ContentWebRTCShim);
addMessageListener("rtcpeer:Deny", ContentWebRTCShim);
addMessageListener("webrtc:Allow", ContentWebRTCShim);
addMessageListener("webrtc:Deny", ContentWebRTCShim);
addMessageListener("webrtc:StopSharing", ContentWebRTCShim);

addEventListener("pageshow", function(event) {
  if (event.target == content.document) {
    sendAsyncMessage("PageVisibility:Show", {
      persisted: event.persisted,
    });
  }
});
addEventListener("pagehide", function(event) {
  if (event.target == content.document) {
    sendAsyncMessage("PageVisibility:Hide", {
      persisted: event.persisted,
    });
  }
});

var PageMetadataMessenger = {
  init() {
    addMessageListener("PageMetadata:GetPageData", this);
    addMessageListener("PageMetadata:GetMicroformats", this);
  },
  receiveMessage(message) {
    switch (message.name) {
      case "PageMetadata:GetPageData": {
        let target = contextMenu.getTarget(message);
        let result = PageMetadata.getData(content.document, target);
        sendAsyncMessage("PageMetadata:PageDataResult", result);
        break;
      }
      case "PageMetadata:GetMicroformats": {
        let target = contextMenu.getTarget(message);
        let result = PageMetadata.getMicroformats(content.document, target);
        sendAsyncMessage("PageMetadata:MicroformatsResult", result);
        break;
      }
    }
  }
};
PageMetadataMessenger.init();

addMessageListener("Bookmarks:GetPageDetails", (message) => {
  let doc = content.document;
  let isErrorPage = /^about:(neterror|certerror|blocked)/.test(doc.documentURI);
  sendAsyncMessage("Bookmarks:GetPageDetails:Result",
                   { isErrorPage,
                     description: PlacesUIUtils.getDescriptionFromDocument(doc) });
});

var LightWeightThemeWebInstallListener = {
  _previewWindow: null,

  init() {
    addEventListener("InstallBrowserTheme", this, false, true);
    addEventListener("PreviewBrowserTheme", this, false, true);
    addEventListener("ResetBrowserThemePreview", this, false, true);
  },

  handleEvent(event) {
    switch (event.type) {
      case "InstallBrowserTheme": {
        sendAsyncMessage("LightWeightThemeWebInstaller:Install", {
          baseURI: event.target.baseURI,
          principal: event.target.nodePrincipal,
          themeData: event.target.getAttribute("data-browsertheme"),
        });
        break;
      }
      case "PreviewBrowserTheme": {
        sendAsyncMessage("LightWeightThemeWebInstaller:Preview", {
          baseURI: event.target.baseURI,
          principal: event.target.nodePrincipal,
          themeData: event.target.getAttribute("data-browsertheme"),
        });
        this._previewWindow = event.target.ownerGlobal;
        this._previewWindow.addEventListener("pagehide", this, true);
        break;
      }
      case "pagehide": {
        sendAsyncMessage("LightWeightThemeWebInstaller:ResetPreview");
        this._resetPreviewWindow();
        break;
      }
      case "ResetBrowserThemePreview": {
        if (this._previewWindow) {
          sendAsyncMessage("LightWeightThemeWebInstaller:ResetPreview",
                           {principal: event.target.nodePrincipal});
          this._resetPreviewWindow();
        }
        break;
      }
    }
  },

  _resetPreviewWindow() {
    this._previewWindow.removeEventListener("pagehide", this, true);
    this._previewWindow = null;
  }
};

LightWeightThemeWebInstallListener.init();

var PageInfoListener = {

  init() {
    addMessageListener("PageInfo:getData", this);
  },

  receiveMessage(message) {
    let strings = message.data.strings;
    let window;
    let document;

    let frameOuterWindowID = message.data.frameOuterWindowID;

    // If inside frame then get the frame's window and document.
    if (frameOuterWindowID != undefined) {
      window = Services.wm.getOuterWindowWithId(frameOuterWindowID);
      document = window.document;
    } else {
      window = content.window;
      document = content.document;
    }

    let pageInfoData = {metaViewRows: this.getMetaInfo(document),
                        docInfo: this.getDocumentInfo(document),
                        feeds: this.getFeedsInfo(document, strings),
                        windowInfo: this.getWindowInfo(window)};

    sendAsyncMessage("PageInfo:data", pageInfoData);

    // Separate step so page info dialog isn't blank while waiting for this to finish.
    this.getMediaInfo(document, window, strings);
  },

  getMetaInfo(document) {
    let metaViewRows = [];

    // Get the meta tags from the page.
    let metaNodes = document.getElementsByTagName("meta");

    for (let metaNode of metaNodes) {
      metaViewRows.push([metaNode.name || metaNode.httpEquiv || metaNode.getAttribute("property"),
                        metaNode.content]);
    }

    return metaViewRows;
  },

  getWindowInfo(window) {
    let windowInfo = {};
    windowInfo.isTopWindow = window == window.top;

    let hostName = null;
    try {
      hostName = Services.io.newURI(window.location.href).displayHost;
    } catch (exception) { }

    windowInfo.hostName = hostName;
    return windowInfo;
  },

  getDocumentInfo(document) {
    let docInfo = {};
    docInfo.title = document.title;
    docInfo.location = document.location.toString();
    try {
      docInfo.location = Services.io.newURI(document.location.toString()).displaySpec;
    } catch (exception) { }
    docInfo.referrer = document.referrer;
    try {
      if (document.referrer) {
        docInfo.referrer = Services.io.newURI(document.referrer).displaySpec;
      }
    } catch (exception) { }
    docInfo.compatMode = document.compatMode;
    docInfo.contentType = document.contentType;
    docInfo.characterSet = document.characterSet;
    docInfo.lastModified = document.lastModified;
    docInfo.principal = document.nodePrincipal;

    let documentURIObject = {};
    documentURIObject.spec = document.documentURIObject.spec;
    docInfo.documentURIObject = documentURIObject;

    docInfo.isContentWindowPrivate = PrivateBrowsingUtils.isContentWindowPrivate(content);

    return docInfo;
  },

  getFeedsInfo(document, strings) {
    let feeds = [];
    // Get the feeds from the page.
    let linkNodes = document.getElementsByTagName("link");
    let length = linkNodes.length;
    for (let i = 0; i < length; i++) {
      let link = linkNodes[i];
      if (!link.href) {
        continue;
      }
      let rel = link.rel && link.rel.toLowerCase();
      let rels = {};

      if (rel) {
        for (let relVal of rel.split(/\s+/)) {
          rels[relVal] = true;
        }
      }

      if (rels.feed || (link.type && rels.alternate && !rels.stylesheet)) {
        let type = Feeds.isValidFeed(link, document.nodePrincipal, "feed" in rels);
        if (type) {
          type = strings[type] || strings["application/rss+xml"];
          feeds.push([link.title, type, link.href]);
        }
      }
    }
    return feeds;
  },

  // Only called once to get the media tab's media elements from the content page.
  getMediaInfo(document, window, strings) {
    let frameList = this.goThroughFrames(document, window);
    this.processFrames(document, frameList, strings);
  },

  goThroughFrames(document, window) {
    let frameList = [document];
    if (window && window.frames.length > 0) {
      let num = window.frames.length;
      for (let i = 0; i < num; i++) {
        // Recurse through the frames.
        frameList.concat(this.goThroughFrames(window.frames[i].document,
                                              window.frames[i]));
      }
    }
    return frameList;
  },

  async processFrames(document, frameList, strings) {
    let nodeCount = 0;
    for (let doc of frameList) {
      let iterator = doc.createTreeWalker(doc, content.NodeFilter.SHOW_ELEMENT);

      // Goes through all the elements on the doc. imageViewRows takes only the media elements.
      while (iterator.nextNode()) {
        let mediaItems = this.getMediaItems(document, strings, iterator.currentNode);

        if (mediaItems.length) {
          sendAsyncMessage("PageInfo:mediaData",
                           {mediaItems, isComplete: false});
        }

        if (++nodeCount % 500 == 0) {
          // setTimeout every 500 elements so we don't keep blocking the content process.
          await new Promise(resolve => setTimeout(resolve, 10));
        }
      }
    }
    // Send that page info media fetching has finished.
    sendAsyncMessage("PageInfo:mediaData", {isComplete: true});
  },

  getMediaItems(document, strings, elem) {
    // Check for images defined in CSS (e.g. background, borders)
    let computedStyle = elem.ownerGlobal.getComputedStyle(elem);
    // A node can have multiple media items associated with it - for example,
    // multiple background images.
    let mediaItems = [];

    let addImage = (url, type, alt, el, isBg) => {
      let element = this.serializeElementInfo(document, url, type, alt, el, isBg);
      mediaItems.push([url, type, alt, element, isBg]);
    };

    if (computedStyle) {
      let addImgFunc = (label, urls) => {
        for (let url of urls) {
          addImage(url, label, strings.notSet, elem, true);
        }
      };
      // FIXME: This is missing properties. See the implementation of
      // getCSSImageURLs for a list of properties.
      //
      // If you don't care about the message you can also pass "all" here and
      // get all the ones the browser knows about.
      addImgFunc(strings.mediaBGImg, computedStyle.getCSSImageURLs("background-image"));
      addImgFunc(strings.mediaBorderImg, computedStyle.getCSSImageURLs("border-image-source"));
      addImgFunc(strings.mediaListImg, computedStyle.getCSSImageURLs("list-style-image"));
      addImgFunc(strings.mediaCursor, computedStyle.getCSSImageURLs("cursor"));
    }

    // One swi^H^H^Hif-else to rule them all.
    if (elem instanceof content.HTMLImageElement) {
      addImage(elem.src, strings.mediaImg,
               (elem.hasAttribute("alt")) ? elem.alt : strings.notSet, elem, false);
    } else if (elem instanceof content.SVGImageElement) {
      try {
        // Note: makeURLAbsolute will throw if either the baseURI is not a valid URI
        //       or the URI formed from the baseURI and the URL is not a valid URI.
        if (elem.href.baseVal) {
          let href = Services.io.newURI(elem.href.baseVal, null, Services.io.newURI(elem.baseURI)).spec;
          addImage(href, strings.mediaImg, "", elem, false);
        }
      } catch (e) { }
    } else if (elem instanceof content.HTMLVideoElement) {
      addImage(elem.currentSrc, strings.mediaVideo, "", elem, false);
    } else if (elem instanceof content.HTMLAudioElement) {
      addImage(elem.currentSrc, strings.mediaAudio, "", elem, false);
    } else if (elem instanceof content.HTMLLinkElement) {
      if (elem.rel && /\bicon\b/i.test(elem.rel)) {
        addImage(elem.href, strings.mediaLink, "", elem, false);
      }
    } else if (elem instanceof content.HTMLInputElement || elem instanceof content.HTMLButtonElement) {
      if (elem.type.toLowerCase() == "image") {
        addImage(elem.src, strings.mediaInput,
                 (elem.hasAttribute("alt")) ? elem.alt : strings.notSet, elem, false);
      }
    } else if (elem instanceof content.HTMLObjectElement) {
      addImage(elem.data, strings.mediaObject, this.getValueText(elem), elem, false);
    } else if (elem instanceof content.HTMLEmbedElement) {
      addImage(elem.src, strings.mediaEmbed, "", elem, false);
    }

    return mediaItems;
  },

  /**
   * Set up a JSON element object with all the instanceOf and other infomation that
   * makePreview in pageInfo.js uses to figure out how to display the preview.
   */

  serializeElementInfo(document, url, type, alt, item, isBG) {
    let result = {};

    let imageText;
    if (!isBG &&
        !(item instanceof content.SVGImageElement) &&
        !(document instanceof content.ImageDocument)) {
      imageText = item.title || item.alt;

      if (!imageText && !(item instanceof content.HTMLImageElement)) {
        imageText = this.getValueText(item);
      }
    }

    result.imageText = imageText;
    result.longDesc = item.longDesc;
    result.numFrames = 1;

    if (item instanceof content.HTMLObjectElement ||
      item instanceof content.HTMLEmbedElement ||
      item instanceof content.HTMLLinkElement) {
      result.mimeType = item.type;
    }

    if (!result.mimeType && !isBG && item instanceof Ci.nsIImageLoadingContent) {
      // Interface for image loading content.
      let imageRequest = item.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);
      if (imageRequest) {
        result.mimeType = imageRequest.mimeType;
        let image = !(imageRequest.imageStatus & imageRequest.STATUS_ERROR) && imageRequest.image;
        if (image) {
          result.numFrames = image.numFrames;
        }
      }
    }

    // If we have a data url, get the MIME type from the url.
    if (!result.mimeType && url.startsWith("data:")) {
      let dataMimeType = /^data:(image\/[^;,]+)/i.exec(url);
      if (dataMimeType)
        result.mimeType = dataMimeType[1].toLowerCase();
    }

    result.HTMLLinkElement = item instanceof content.HTMLLinkElement;
    result.HTMLInputElement = item instanceof content.HTMLInputElement;
    result.HTMLImageElement = item instanceof content.HTMLImageElement;
    result.HTMLObjectElement = item instanceof content.HTMLObjectElement;
    result.SVGImageElement = item instanceof content.SVGImageElement;
    result.HTMLVideoElement = item instanceof content.HTMLVideoElement;
    result.HTMLAudioElement = item instanceof content.HTMLAudioElement;

    if (isBG) {
      // Items that are showing this image as a background
      // image might not necessarily have a width or height,
      // so we'll dynamically generate an image and send up the
      // natural dimensions.
      let img = content.document.createElement("img");
      img.src = url;
      result.naturalWidth = img.naturalWidth;
      result.naturalHeight = img.naturalHeight;
    } else {
      // Otherwise, we can use the current width and height
      // of the image.
      result.width = item.width;
      result.height = item.height;
    }

    if (item instanceof content.SVGImageElement) {
      result.SVGImageElementWidth = item.width.baseVal.value;
      result.SVGImageElementHeight = item.height.baseVal.value;
    }

    result.baseURI = item.baseURI;

    return result;
  },

  // Other Misc Stuff
  // Modified from the Links Panel v2.3, http://segment7.net/mozilla/links/links.html
  // parse a node to extract the contents of the node
  getValueText(node) {

    let valueText = "";

    // Form input elements don't generally contain information that is useful to our callers, so return nothing.
    if (node instanceof content.HTMLInputElement ||
        node instanceof content.HTMLSelectElement ||
        node instanceof content.HTMLTextAreaElement) {
      return valueText;
    }

    // Otherwise recurse for each child.
    let length = node.childNodes.length;

    for (let i = 0; i < length; i++) {
      let childNode = node.childNodes[i];
      let nodeType = childNode.nodeType;

      // Text nodes are where the goods are.
      if (nodeType == content.Node.TEXT_NODE) {
        valueText += " " + childNode.nodeValue;
      } else if (nodeType == content.Node.ELEMENT_NODE) {
        // And elements can have more text inside them.
        // Images are special, we want to capture the alt text as if the image weren't there.
        if (childNode instanceof content.HTMLImageElement) {
          valueText += " " + this.getAltText(childNode);
        } else {
          valueText += " " + this.getValueText(childNode);
        }
      }
    }

    return this.stripWS(valueText);
  },

  // Copied from the Links Panel v2.3, http://segment7.net/mozilla/links/links.html.
  // Traverse the tree in search of an img or area element and grab its alt tag.
  getAltText(node) {
    let altText = "";

    if (node.alt) {
      return node.alt;
    }
    let length = node.childNodes.length;
    for (let i = 0; i < length; i++) {
      if ((altText = this.getAltText(node.childNodes[i]) != undefined)) { // stupid js warning...
        return altText;
      }
    }
    return "";
  },

  // Copied from the Links Panel v2.3, http://segment7.net/mozilla/links/links.html.
  // Strip leading and trailing whitespace, and replace multiple consecutive whitespace characters with a single space.
  stripWS(text) {
    let middleRE = /\s+/g;
    let endRE = /(^\s+)|(\s+$)/g;

    text = text.replace(middleRE, " ");
    return text.replace(endRE, "");
  }
};
PageInfoListener.init();

let OfflineApps = {
  _docId: 0,
  _docIdMap: new Map(),

  _docManifestSet: new Set(),

  _observerAdded: false,
  registerWindow(aWindow) {
    if (!this._observerAdded) {
      this._observerAdded = true;
      Services.obs.addObserver(this, "offline-cache-update-completed", true);
    }
    let manifestURI = this._getManifestURI(aWindow);
    this._docManifestSet.add(manifestURI.spec);
  },

  handleEvent(event) {
    if (event.type == "MozApplicationManifest") {
      this.offlineAppRequested(event.originalTarget.defaultView);
    }
  },

  _getManifestURI(aWindow) {
    if (!aWindow.document.documentElement)
      return null;

    var attr = aWindow.document.documentElement.getAttribute("manifest");
    if (!attr)
      return null;

    try {
      return Services.io.newURI(attr, aWindow.document.characterSet,
                                Services.io.newURI(aWindow.location.href));
    } catch (e) {
      return null;
    }
  },

  offlineAppRequested(aContentWindow) {
    this.registerWindow(aContentWindow);
    if (!Services.prefs.getBoolPref("browser.offline-apps.notify")) {
      return;
    }

    let currentURI = aContentWindow.document.documentURIObject;
    // don't bother showing UI if the user has already made a decision
    if (Services.perms.testExactPermission(currentURI, "offline-app") != Services.perms.UNKNOWN_ACTION)
      return;

    try {
      if (Services.prefs.getBoolPref("offline-apps.allow_by_default")) {
        // all pages can use offline capabilities, no need to ask the user
        return;
      }
    } catch (e) {
      // this pref isn't set by default, ignore failures
    }
    let docId = ++this._docId;
    this._docIdMap.set(docId, Cu.getWeakReference(aContentWindow.document));
    sendAsyncMessage("OfflineApps:RequestPermission", {
      uri: currentURI.spec,
      docId,
    });
  },

  _startFetching(aDocument) {
    if (!aDocument.documentElement)
      return;

    let manifestURI = this._getManifestURI(aDocument.defaultView);
    if (!manifestURI)
      return;

    var updateService = Cc["@mozilla.org/offlinecacheupdate-service;1"].
                        getService(Ci.nsIOfflineCacheUpdateService);
    updateService.scheduleUpdate(manifestURI, aDocument.documentURIObject,
                                 aDocument.nodePrincipal, aDocument.defaultView);
  },

  receiveMessage(aMessage) {
    if (aMessage.name == "OfflineApps:StartFetching") {
      let doc = this._docIdMap.get(aMessage.data.docId);
      doc = doc && doc.get();
      if (doc) {
        this._startFetching(doc);
      }
      this._docIdMap.delete(aMessage.data.docId);
    }
  },

  observe(aSubject, aTopic, aState) {
    if (aTopic == "offline-cache-update-completed") {
      let cacheUpdate = aSubject.QueryInterface(Ci.nsIOfflineCacheUpdate);
      let uri = cacheUpdate.manifestURI;
      if (uri && this._docManifestSet.has(uri.spec)) {
        sendAsyncMessage("OfflineApps:CheckUsage", {uri: uri.spec});
      }
    }
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsISupportsWeakReference]),
};

addEventListener("MozApplicationManifest", OfflineApps, false);
addMessageListener("OfflineApps:StartFetching", OfflineApps);
