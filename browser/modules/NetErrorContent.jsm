/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["NetErrorContent"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "BrowserUtils",
                               "resource://gre/modules/BrowserUtils.jsm");
ChromeUtils.defineModuleGetter(this, "WebNavigationFrames",
                               "resource://gre/modules/WebNavigationFrames.jsm");

XPCOMUtils.defineLazyGetter(this, "gPipNSSBundle", function() {
  return Services.strings.createBundle("chrome://pipnss/locale/pipnss.properties");
});
XPCOMUtils.defineLazyGetter(this, "gNSSErrorsBundle", function() {
  return Services.strings.createBundle("chrome://pipnss/locale/nsserrors.properties");
});


const SEC_ERROR_BASE          = Ci.nsINSSErrorsService.NSS_SEC_ERROR_BASE;
const MOZILLA_PKIX_ERROR_BASE = Ci.nsINSSErrorsService.MOZILLA_PKIX_ERROR_BASE;

const SEC_ERROR_EXPIRED_CERTIFICATE                = SEC_ERROR_BASE + 11;
const SEC_ERROR_UNKNOWN_ISSUER                     = SEC_ERROR_BASE + 13;
const SEC_ERROR_UNTRUSTED_ISSUER                   = SEC_ERROR_BASE + 20;
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

var NetErrorContent = {
  isAboutNetError(doc) {
    return doc.documentURI.startsWith("about:neterror");
  },

  isAboutCertError(doc) {
    return doc.documentURI.startsWith("about:certerror");
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

  onCertErrorDetails(global, msg, docShell) {
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

  handleEvent(aGlobal, aEvent) {
    // Documents have a null ownerDocument.
    let doc = aEvent.originalTarget.ownerDocument || aEvent.originalTarget;

    switch (aEvent.type) {
    case "AboutNetErrorLoad":
      this.onPageLoad(aGlobal, aEvent.originalTarget, doc.defaultView);
      break;
    case "AboutNetErrorOpenCaptivePortal":
      this.openCaptivePortalPage(aGlobal, aEvent);
      break;
    case "AboutNetErrorSetAutomatic":
      this.onSetAutomatic(aGlobal, aEvent);
      break;
    case "AboutNetErrorResetPreferences":
      this.onResetPreferences(aGlobal, aEvent);
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

  onPageLoad(global, originalTarget, win) {
    // Values for telemtery bins: see TLS_ERROR_REPORT_UI in Histograms.json
    const TLS_ERROR_REPORT_TELEMETRY_UI_SHOWN = 0;

    let hideAddExceptionButton = false;

    if (this.isAboutCertError(win.document)) {
      this.onCertError(global, originalTarget, win);
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

    global.sendAsyncMessage("Browser:SSLErrorReportTelemetry",
                            {reportStatus: TLS_ERROR_REPORT_TELEMETRY_UI_SHOWN});
  },

  openCaptivePortalPage(global, evt) {
    global.sendAsyncMessage("Browser:OpenCaptivePortalPage");
  },


  onResetPreferences(global, evt) {
    global.sendAsyncMessage("Browser:ResetSSLPreferences");
  },

  onSetAutomatic(global, evt) {
    global.sendAsyncMessage("Browser:SetSSLErrorReportAuto", {
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

  onCertError(global, targetElement, win) {
    let docShell = win.document.docShell;
    global.sendAsyncMessage("Browser:CertExceptionError", {
      frameId: WebNavigationFrames.getFrameId(win),
      location: win.document.location.href,
      elementId: targetElement.getAttribute("id"),
      isTopFrame: (win.parent === win),
      securityInfoAsString: getSerializedSecurityInfo(docShell),
    });
  },

  onAboutNetError(global, event, documentURI) {
    let elmId = event.originalTarget.getAttribute("id");
    if (elmId == "returnButton") {
      global.sendAsyncMessage("Browser:SSLErrorGoBack", {});
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
      global.sendAsyncMessage("Browser:EnableOnlineMode", {});
    }
  },
};
