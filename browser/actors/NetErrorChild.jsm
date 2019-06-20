/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["NetErrorChild"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {ActorChild} = ChromeUtils.import("resource://gre/modules/ActorChild.jsm");

ChromeUtils.defineModuleGetter(this, "BrowserUtils",
                               "resource://gre/modules/BrowserUtils.jsm");
ChromeUtils.defineModuleGetter(this, "WebNavigationFrames",
                               "resource://gre/modules/WebNavigationFrames.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

XPCOMUtils.defineLazyGetter(this, "gPipNSSBundle", function() {
  return Services.strings.createBundle("chrome://pipnss/locale/pipnss.properties");
});
XPCOMUtils.defineLazyGetter(this, "gBrandBundle", function() {
  return Services.strings.createBundle("chrome://branding/locale/brand.properties");
});
XPCOMUtils.defineLazyPreferenceGetter(this, "mitmErrorPageEnabled",
  "browser.security.newcerterrorpage.mitm.enabled");
XPCOMUtils.defineLazyPreferenceGetter(this, "mitmPrimingEnabled",
  "security.certerrors.mitm.priming.enabled");
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
const SEC_ERROR_REUSED_ISSUER_AND_SERIAL           = SEC_ERROR_BASE + 138;
const SEC_ERROR_OCSP_INVALID_SIGNING_CERT          = SEC_ERROR_BASE + 144;
const SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED  = SEC_ERROR_BASE + 176;
const MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE = MOZILLA_PKIX_ERROR_BASE + 5;
const MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE = MOZILLA_PKIX_ERROR_BASE + 6;
const MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED = MOZILLA_PKIX_ERROR_BASE + 13;
const MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT          = MOZILLA_PKIX_ERROR_BASE + 14;
const MOZILLA_PKIX_ERROR_MITM_DETECTED             = MOZILLA_PKIX_ERROR_BASE + 15;


const SSL_ERROR_BASE = Ci.nsINSSErrorsService.NSS_SSL_ERROR_BASE;
const SSL_ERROR_BAD_CERT_DOMAIN = SSL_ERROR_BASE + 12;
const SSL_ERROR_SSL_DISABLED  = SSL_ERROR_BASE + 20;
const SSL_ERROR_SSL2_DISABLED  = SSL_ERROR_BASE + 14;

const PREF_SERVICES_SETTINGS_CLOCK_SKEW_SECONDS = "services.settings.clock_skew_seconds";
const PREF_SERVICES_SETTINGS_LAST_FETCHED       = "services.settings.last_update_seconds";

const PREF_SSL_IMPACT_ROOTS = ["security.tls.version.", "security.ssl3."];

let formatter = new Services.intl.DateTimeFormat(undefined, {
  dateStyle: "long",
});

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

class NetErrorChild extends ActorChild {
  isAboutNetError(doc) {
    return doc.documentURI.startsWith("about:neterror");
  }

  isAboutCertError(doc) {
    return doc.documentURI.startsWith("about:certerror");
  }

  getParams(doc) {
    let searchParams = new URL(doc.documentURI).searchParams;
    return {
      cssClass: searchParams.get("s"),
      error: searchParams.get("e"),
    };
  }

  _getCertValidityRange(docShell) {
    let {securityInfo} = docShell.failedChannel;
    securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
    let notBefore = 0;
    let notAfter = Number.MAX_SAFE_INTEGER;
    for (let cert of securityInfo.failedCertChain.getEnumerator()) {
      notBefore = Math.max(notBefore, cert.validity.notBefore);
      notAfter = Math.min(notAfter, cert.validity.notAfter);
    }
    // nsIX509Cert reports in PR_Date terms, which uses microseconds. Convert:
    notBefore /= 1000;
    notAfter /= 1000;
    return {notBefore, notAfter};
  }

  _setTechDetails(input, doc) {
    // CSS class and error code are set from nsDocShell.
    let {cssClass, error} = this.getParams(doc);
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
        case MOZILLA_PKIX_ERROR_MITM_DETECTED:
          if (mitmErrorPageEnabled) {
            let brandName = gBrandBundle.GetStringFromName("brandShortName");
            msg1 = gPipNSSBundle.GetStringFromName("certErrorMitM");
            msg1 += "\n\n";
            msg1 += gPipNSSBundle.formatStringFromName("certErrorMitM2", [brandName], 1);
            msg1 += "\n\n";
            msg1 += gPipNSSBundle.formatStringFromName("certErrorMitM3", [brandName], 1);
            msg1 += "\n";
            break;
          }
          // If the condition is false, fall through...
        case SEC_ERROR_UNKNOWN_ISSUER:
          let brandName = gBrandBundle.GetStringFromName("brandShortName");
          msg1 = "";
          msg1 += gPipNSSBundle.formatStringFromName("certErrorTrust_UnknownIssuer4", [hostString], 1);
          msg1 += "\n\n";
          msg1 += gPipNSSBundle.formatStringFromName("certErrorTrust_UnknownIssuer6", [brandName, hostString], 2);
          msg1 += "\n\n";
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
        // This error code currently only exists for the Symantec distrust, we may need to adjust
        // it to fit other distrusts later.
        case MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED:
          msg1 += gPipNSSBundle.GetStringFromName("certErrorTrust_Symantec1") + "\n";
          break;
        default:
          msg1 += gPipNSSBundle.GetStringFromName("certErrorTrust_Untrusted") + "\n";
      }
    }

    technicalInfo.appendChild(doc.createTextNode(msg1));

    if (input.data.isDomainMismatch) {
      let subjectAltNames = input.data.certSubjectAltNames.split(",");
      let numSubjectAltNames = subjectAltNames.length;

      subjectAltNames = subjectAltNames.filter(name => name.length > 0);
      numSubjectAltNames = subjectAltNames.length;

      let msgPrefix = "";
      if (numSubjectAltNames != 0) {
        if (numSubjectAltNames == 1) {
          technicalInfo.textContent = "";
          let brandName = gBrandBundle.GetStringFromName("brandShortName");
          msgPrefix = gPipNSSBundle.formatStringFromName("certErrorMismatchSinglePrefix3", [brandName, hostString], 2) + " ";
          msgPrefix += gPipNSSBundle.GetStringFromName("certErrorMismatchSinglePrefix");
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
          let msg = "";
          technicalInfo.textContent = "";
          let brandName = gBrandBundle.GetStringFromName("brandShortName");
          msg = gPipNSSBundle.formatStringFromName("certErrorMismatchMultiple3", [brandName, hostString], 2) + " ";
          for (let i = 0; i < numSubjectAltNames; i++) {
            msg += subjectAltNames[i];
            if (i != (numSubjectAltNames - 1)) {
              msg += ", ";
            }
          }
          technicalInfo.append(msg + "\n");
        }
      } else {
        let msg = "";
        technicalInfo.textContent = "";
        let brandName = gBrandBundle.GetStringFromName("brandShortName");
        msg = gPipNSSBundle.formatStringFromName("certErrorMismatch3", [brandName, hostString], 2) + " ";
        technicalInfo.append(msg + "\n");
      }
    }

    if (input.data.isNotValidAtThisTime) {
      let msg;
      if (input.data.validity.notBefore && (Date.now() < input.data.validity.notAfter)) {
        let notBeforeLocalTime = formatter.format(new Date(input.data.validity.notBefore));
        msg = gPipNSSBundle.formatStringFromName("certErrorNotYetValidNow3",
                                                 [hostString, notBeforeLocalTime], 2);
      } else {
        let notAfterLocalTime = formatter.format(new Date(input.data.validity.notAfter));
        msg = gPipNSSBundle.formatStringFromName("certErrorExpiredNow3",
                                                 [hostString, notAfterLocalTime], 2);
      }
      msg += "\n";

      technicalInfo.textContent = "";
      technicalInfo.append(msg);
    }
    technicalInfo.append("\n");

    // Add link to certificate and error message.
    let linkPrefix = gPipNSSBundle.GetStringFromName("certErrorCodePrefix3");
    let detailLink = doc.createElement("a");
    detailLink.append(input.data.codeString);
    detailLink.title = input.data.codeString;
    detailLink.id = "errorCode";
    detailLink.dataset.telemetryId = "error_code_link";
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
  }

  // eslint-disable-next-line complexity
  onCertErrorDetails(msg, docShell) {
    let doc = docShell.document;

    // This function centers the error container after its content updates.
    // It is currently duplicated in aboutNetError.js to avoid having to do
    // async communication to the page that would result in flicker.
    // TODO(johannh): Get rid of this duplication.
    function updateContainerPosition() {
      let textContainer = doc.getElementById("text-container");
      // Using the vh CSS property our margin adapts nicely to window size changes.
      // Unfortunately, this doesn't work correctly in iframes, which is why we need
      // to manually compute the height there.
      if (doc.ownerGlobal.parent == doc.ownerGlobal) {
        textContainer.style.marginTop = `calc(50vh - ${textContainer.clientHeight / 2}px)`;
      } else {
        let offset = (doc.documentElement.clientHeight / 2) - (textContainer.clientHeight / 2);
        if (offset > 0) {
          textContainer.style.marginTop = `${offset}px`;
        }
      }
    }

    // Check if the connection is being man-in-the-middled. When the parent
    // detects an intercepted connection, the page may be reloaded with a new
    // error code (MOZILLA_PKIX_ERROR_MITM_DETECTED).
    if (mitmPrimingEnabled && msg.data.code == SEC_ERROR_UNKNOWN_ISSUER &&
        // Only do this check for top-level failures.
        doc.ownerGlobal.top === doc.ownerGlobal) {
      this.mm.sendAsyncMessage("Browser:PrimeMitm");
    }

    let div = doc.getElementById("certificateErrorText");
    div.textContent = msg.data.info;
    this._setTechDetails(msg, doc);
    let learnMoreLink = doc.getElementById("learnMoreLink");
    let baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");
    learnMoreLink.setAttribute("href", baseURL + "connection-not-secure");
    let errWhatToDo = doc.getElementById("es_nssBadCert_" + msg.data.codeString);
    let es = doc.getElementById("errorWhatToDoText");
    let errWhatToDoTitle = doc.getElementById("edd_nssBadCert");
    let est = doc.getElementById("errorWhatToDoTitleText");
    let searchParams = new URLSearchParams(doc.documentURI.split("?")[1]);
    let error = searchParams.get("e");

    if (error == "sslv3Used") {
      learnMoreLink.setAttribute("href", baseURL + "sslv3-error-messages");
    }

    if (error == "nssFailure2") {
      let shortDesc = doc.getElementById("errorShortDescText").textContent;
      // nssFailure2 also gets us other non-overrideable errors. Choose
      // a "learn more" link based on description:
      if (shortDesc.includes("MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE")) {
        learnMoreLink.setAttribute("href", baseURL + "certificate-pinning-reports");
      }
    }

    // This is set to true later if the user's system clock is at fault for this error.
    let clockSkew = false;

    doc.body.setAttribute("code", msg.data.codeString);

    // Need to do this here (which is not exactly at load but a few ticks later),
    // because this is the first time we have access to the error code.
    this.recordLoadEvent(doc);

    switch (msg.data.code) {
      case SSL_ERROR_BAD_CERT_DOMAIN:
      case SEC_ERROR_OCSP_INVALID_SIGNING_CERT:
      case SEC_ERROR_UNKNOWN_ISSUER:
        if (es) {
          // eslint-disable-next-line no-unsanitized/property
          es.innerHTML = errWhatToDo.innerHTML;
        }
        if (est) {
          // eslint-disable-next-line no-unsanitized/property
          est.innerHTML = errWhatToDoTitle.innerHTML;
        }
        updateContainerPosition();
        break;

      // This error code currently only exists for the Symantec distrust
      // in Firefox 63, so we add copy explaining that to the user.
      // In case of future distrusts of that scale we might need to add
      // additional parameters that allow us to identify the affected party
      // without replicating the complex logic from certverifier code.
      case MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED:
        let description = gPipNSSBundle.formatStringFromName(
          "certErrorSymantecDistrustDescription1", [doc.location.hostname], 1);
        let descriptionContainer = doc.getElementById("errorShortDescText2");
        descriptionContainer.textContent = description;

        let adminDescription = doc.createElement("p");
        adminDescription.textContent =
          gPipNSSBundle.GetStringFromName("certErrorSymantecDistrustAdministrator");
        descriptionContainer.append(adminDescription);

        learnMoreLink.href = baseURL + "symantec-warning";

        updateContainerPosition();
        break;
      case MOZILLA_PKIX_ERROR_MITM_DETECTED:
        if (mitmErrorPageEnabled) {
          let autoEnabledEnterpriseRoots =
            Services.prefs.getBoolPref("security.enterprise_roots.auto-enabled", false);
          if (mitmPrimingEnabled && autoEnabledEnterpriseRoots) {
            // If we automatically tried to import enterprise root certs but it didn't
            // fix the MITM, reset the pref.
            this.mm.sendAsyncMessage("Browser:ResetEnterpriseRootsPref");
          }

          // We don't actually know what the MitM is called (since we don't
          // maintain a list), so we'll try and display the common name of the
          // root issuer to the user. In the worst case they are as clueless as
          // before, in the best case this gives them an actionable hint.
          // This may be revised in the future.
          let {securityInfo} = docShell.failedChannel;
          securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
          let mitmName = null;
          for (let cert of securityInfo.failedCertChain.getEnumerator()) {
            mitmName = cert.issuerCommonName;
          }
          for (let span of doc.querySelectorAll(".mitm-name")) {
            span.textContent = mitmName;
          }

          learnMoreLink.href = baseURL + "security-error";

          let title = doc.getElementById("et_mitm");
          let desc = doc.getElementById("ed_mitm");
          doc.querySelector(".title-text").textContent = title.textContent;
          // eslint-disable-next-line no-unsanitized/property
          doc.getElementById("errorShortDescText").innerHTML = desc.innerHTML;

          // eslint-disable-next-line no-unsanitized/property
          es.innerHTML = errWhatToDo.innerHTML;
          // eslint-disable-next-line no-unsanitized/property
          est.innerHTML = errWhatToDoTitle.innerHTML;

          updateContainerPosition();
          break;
        }
        // If the condition is false, fall through...
      case MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT:
        learnMoreLink.href = baseURL + "security-error";
        break;

      // In case the certificate expired we make sure the system clock
      // matches the remote-settings service (blocklist via Kinto) ping time
      // and is not before the build date.
      case SEC_ERROR_EXPIRED_CERTIFICATE:
      case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
      case MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE:
      case MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE:

        learnMoreLink.href = baseURL + "time-errors";
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
          clockSkew = true;

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
            clockSkew = true;
          }
        }

        let systemDate = formatter.format(new Date());
        doc.getElementById("wrongSystemTime_systemDate1").textContent = systemDate;
        if (clockSkew) {
          doc.body.classList.add("illustrated", "clockSkewError");
          let clockErrTitle = doc.getElementById("et_clockSkewError");
          let clockErrDesc = doc.getElementById("ed_clockSkewError");
          // eslint-disable-next-line no-unsanitized/property
          doc.querySelector(".title-text").textContent = clockErrTitle.textContent;
          let desc = doc.getElementById("errorShortDescText");
          doc.getElementById("errorShortDesc").style.display = "block";
          doc.getElementById("certificateErrorReporting").style.display = "none";
          if (desc) {
            // eslint-disable-next-line no-unsanitized/property
            desc.innerHTML = clockErrDesc.innerHTML;
          }
          let errorPageContainer = doc.getElementById("errorPageContainer");
          let textContainer = doc.getElementById("text-container");
          errorPageContainer.style.backgroundPosition = `left top calc(50vh - ${textContainer.clientHeight / 2}px)`;
        } else {
          let targetElems = doc.querySelectorAll("#wrongSystemTime_systemDate2");
          for (let elem of targetElems) {
            elem.textContent = systemDate;
          }

          let errDesc = doc.getElementById("ed_nssBadCert_SEC_ERROR_EXPIRED_CERTIFICATE");
          let sd = doc.getElementById("errorShortDescText");
          // eslint-disable-next-line no-unsanitized/property
          sd.innerHTML = errDesc.innerHTML;

          let span = sd.querySelector(".hostname");
          span.textContent = doc.location.hostname;

          // The secondary description mentions expired certificates explicitly
          // and should only be shown if the certificate has actually expired
          // instead of being not yet valid.
          if (msg.data.code == SEC_ERROR_EXPIRED_CERTIFICATE) {
            let {cssClass} = this.getParams(doc);
            let stsSuffix = cssClass == "badStsCert" ? "_sts" : "";
            let errDesc2 = doc.getElementById(
              `ed2_nssBadCert_SEC_ERROR_EXPIRED_CERTIFICATE${stsSuffix}`);
            let sd2 = doc.getElementById("errorShortDescText2");
            // eslint-disable-next-line no-unsanitized/property
            sd2.innerHTML = errDesc2.innerHTML;
          }

          if (es) {
            // eslint-disable-next-line no-unsanitized/property
            es.innerHTML = errWhatToDo.innerHTML;
          }
          if (est) {
            // eslint-disable-next-line no-unsanitized/property
            est.textContent = errWhatToDoTitle.textContent;
            est.style.fontWeight = "bold";
          }
          updateContainerPosition();
        }
        break;
    }

    // Add slightly more alarming UI unless there are indicators that
    // show that the error is harmless or can not be skipped anyway.
    let {cssClass} = this.getParams(doc);
    // Don't alarm users when they can't continue to the website anyway...
    if (cssClass != "badStsCert" &&
        // Errors in iframes can't be skipped either...
        doc.ownerGlobal.parent == doc.ownerGlobal &&
        // Also don't bother if it's just the user's clock being off...
        !clockSkew &&
        // Symantec distrust is likely harmless as well.
        msg.data.code != MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED) {
      doc.body.classList.add("caution");
    }
  }

  handleEvent(aEvent) {
    // Documents have a null ownerDocument.
    let doc = aEvent.originalTarget.ownerDocument || aEvent.originalTarget;

    switch (aEvent.type) {
    case "AboutNetErrorLoad":
      this.onPageLoad(aEvent.originalTarget, doc.defaultView);
      break;
    case "AboutNetErrorSetAutomatic":
      this.onSetAutomatic(aEvent);
      break;
    case "AboutNetErrorResetPreferences":
      this.onResetPreferences(aEvent);
      break;
    case "click":
      if (aEvent.button == 0) {
        if (this.isAboutCertError(doc)) {
          this.recordClick(aEvent.originalTarget);
          this.onCertError(aEvent.originalTarget, doc.defaultView);
        } else {
          this.onClick(aEvent);
        }
      } else if (this.isAboutCertError(doc)) {
          this.recordClick(aEvent.originalTarget);
      }
      break;
    }
  }

  receiveMessage(msg) {
    if (msg.name == "CertErrorDetails") {
      let frameDocShell = WebNavigationFrames.findDocShell(msg.data.frameId, this.docShell);
      // We need nsIWebNavigation to access docShell.document.
      frameDocShell && frameDocShell.QueryInterface(Ci.nsIWebNavigation);
      if (!frameDocShell || !this.isAboutCertError(frameDocShell.document)) {
        return;
      }

      this.onCertErrorDetails(msg, frameDocShell);
    }
  }

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
  }

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
    try {
      if (id_str) {
        msg2 = gPipNSSBundle.GetStringFromName(id_str) + "\n";
      } else if (nss_error_id_str) {
        msg2 = gNSSErrorsBundle.GetStringFromName(nss_error_id_str) + "\n";
      }
    } catch (e) {
      msg2 = "";
    }

    if (!msg2) {
      // We couldn't get an error message. Use the error string.
      // Note that this is different from before where we used PR_ErrorToString.
      msg2 = nss_error_id_str;
    }
    let msg = gPipNSSBundle.formatStringFromName("SSLConnectionErrorPrefix2",
                                                 [hostString, msg2], 2);

    if (nss_error_id_str && msg2 != nss_error_id_str) {
      msg += gPipNSSBundle.formatStringFromName("certErrorCodePrefix3",
                                                [nss_error_id_str], 1) + "\n";
    }
    return msg;
  }

  onPageLoad(originalTarget, win) {
    // Values for telemtery bins: see TLS_ERROR_REPORT_UI in Histograms.json
    const TLS_ERROR_REPORT_TELEMETRY_UI_SHOWN = 0;

    let hideAddExceptionButton = false;

    if (this.isAboutCertError(win.document)) {
      this.onCertError(originalTarget, win);
      hideAddExceptionButton =
        Services.prefs.getBoolPref("security.certerror.hideAddException", false);
    }
    if (this.isAboutNetError(win.document)) {
      let docShell = win.docShell;
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

      let learnMoreLink = win.document.getElementById("learnMoreLink");
      let baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");
      learnMoreLink.setAttribute("href", baseURL + "connection-not-secure");
    }

    let automatic = Services.prefs.getBoolPref("security.ssl.errorReporting.automatic");
    win.dispatchEvent(new win.CustomEvent("AboutNetErrorOptions", {
      detail: JSON.stringify({
        enabled: Services.prefs.getBoolPref("security.ssl.errorReporting.enabled"),
        changedCertPrefs: this.changedCertPrefs(),
        automatic,
        hideAddExceptionButton,
      }),
    }));

    this.mm.sendAsyncMessage("Browser:SSLErrorReportTelemetry",
                            {reportStatus: TLS_ERROR_REPORT_TELEMETRY_UI_SHOWN});
  }

  onResetPreferences(evt) {
    this.mm.sendAsyncMessage("Browser:ResetSSLPreferences");
  }

  onSetAutomatic(evt) {
    this.mm.sendAsyncMessage("Browser:SetSSLErrorReportAuto", {
      automatic: evt.detail,
    });

    // If we're enabling reports, send a report for this failure.
    if (evt.detail) {
      let win = evt.originalTarget.ownerGlobal;
      let docShell = win.docShell;

      let {securityInfo} = docShell.failedChannel;
      securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
      let {host, port} = win.document.mozDocumentURIIfNotForErrorPages;

      let errorReporter = Cc["@mozilla.org/securityreporter;1"]
                            .getService(Ci.nsISecurityReporter);
      errorReporter.reportTLSError(securityInfo, host, port);
    }
  }

  onCertError(target, win) {
    this.mm.sendAsyncMessage("Browser:CertExceptionError", {
      frameId: WebNavigationFrames.getFrameId(win),
      location: win.document.location.href,
      elementId: target.getAttribute("id"),
      isTopFrame: (win.parent === win),
      securityInfoAsString: getSerializedSecurityInfo(win.docShell),
    });
  }

  getCSSClass(doc) {
    let searchParams = new URL(doc.documentURI).searchParams;
    return searchParams.get("s");
  }

  recordLoadEvent(doc) {
    let cssClass = this.getCSSClass(doc);
    // Telemetry values for events are max. 80 bytes.
    let errorCode = doc.body.getAttribute("code").substring(0, 40);
    Services.telemetry.recordEvent("security.ui.certerror", "load", "aboutcerterror", errorCode, {
      "has_sts": (cssClass == "badStsCert").toString(),
      "is_frame": (doc.ownerGlobal.parent != doc.ownerGlobal).toString(),
    });
  }

  recordClick(element) {
    let telemetryId = element.dataset.telemetryId;
    if (!telemetryId) {
      return;
    }
    let doc = element.ownerDocument;
    let cssClass = this.getCSSClass(doc);
    // Telemetry values for events are max. 80 bytes.
    let errorCode = doc.body.getAttribute("code").substring(0, 40);
    let panel = doc.getElementById("badCertAdvancedPanel");
    Services.telemetry.recordEvent("security.ui.certerror", "click", telemetryId, errorCode, {
      "panel_open": (panel.style.display == "none").toString(),
      "has_sts": (cssClass == "badStsCert").toString(),
      "is_frame": (doc.ownerGlobal.parent != doc.ownerGlobal).toString(),
    });
  }

  onClick(event) {
    let {documentURI} = event.target.ownerDocument;

    let elmId = event.originalTarget.getAttribute("id");
    if (elmId == "returnButton") {
      this.mm.sendAsyncMessage("Browser:SSLErrorGoBack", {});
      return;
    }

    if (!event.originalTarget.classList.contains("try-again") || !/e=netOffline/.test(documentURI)) {
      return;
    }
    // browser front end will handle clearing offline mode and refreshing
    // the page *if* we're in offline mode now. Otherwise let the error page
    // handle the click.
    if (Services.io.offline) {
      event.preventDefault();
      this.mm.sendAsyncMessage("Browser:EnableOnlineMode", {});
    }
  }
}
