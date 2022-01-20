/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

import "chrome://global/content/certviewer/pvutils_bundle.js";
import "chrome://global/content/certviewer/asn1js_bundle.js";
import "chrome://global/content/certviewer/pkijs_bundle.js";
import "chrome://global/content/certviewer/certDecoder.js";

const { Integer, fromBER } = globalThis.asn1js.asn1js;
const { Certificate } = globalThis.pkijs.pkijs;
const { fromBase64, stringToArrayBuffer } = globalThis.pvutils.pvutils;
const { parse, pemToDER } = globalThis.certDecoderInitializer(
  Integer,
  fromBER,
  Certificate,
  fromBase64,
  stringToArrayBuffer,
  crypto
);

const formatter = new Intl.DateTimeFormat("default");

const HOST_NAME = new URL(RPMGetInnerMostURI(document.location.href)).hostname;

// Used to check if we have a specific localized message for an error.
const KNOWN_ERROR_TITLE_IDS = new Set([
  // Error titles:
  "connectionFailure-title",
  "deniedPortAccess-title",
  "dnsNotFound-title",
  "fileNotFound-title",
  "fileAccessDenied-title",
  "generic-title",
  "captivePortal-title",
  "malformedURI-title",
  "netInterrupt-title",
  "notCached-title",
  "netOffline-title",
  "contentEncodingError-title",
  "unsafeContentType-title",
  "netReset-title",
  "netTimeout-title",
  "unknownProtocolFound-title",
  "proxyConnectFailure-title",
  "proxyResolveFailure-title",
  "redirectLoop-title",
  "unknownSocketType-title",
  "nssFailure2-title",
  "csp-xfo-error-title",
  "corruptedContentError-title",
  "sslv3Used-title",
  "inadequateSecurityError-title",
  "blockedByPolicy-title",
  "clockSkewError-title",
  "networkProtocolError-title",
  "nssBadCert-title",
  "nssBadCert-sts-title",
  "certerror-mitm-title",
]);

/* The error message IDs from nsserror.ftl get processed into
 * aboutNetErrorCodes.js which is loaded before we are: */
/* global KNOWN_ERROR_MESSAGE_IDS */

// The following parameters are parsed from the error URL:
//   e - the error code
//   s - custom CSS class to allow alternate styling/favicons
//   d - error description
//   captive - "true" to indicate we're behind a captive portal.
//             Any other value is ignored.

// Note that this file uses document.documentURI to get
// the URL (with the format from above). This is because
// document.location.href gets the current URI off the docshell,
// which is the URL displayed in the location bar, i.e.
// the URI that the user attempted to load.

let searchParams = new URLSearchParams(document.documentURI.split("?")[1]);

let gErrorCode = searchParams.get("e");

let gIsCertError = gErrorCode == "nssBadCert";

// If the location of the favicon changes, FAVICON_CERTERRORPAGE_URL and/or
// FAVICON_ERRORPAGE_URL in toolkit/components/places/nsFaviconService.idl
// should also be updated.
document.getElementById("favicon").href =
  gIsCertError || gErrorCode == "nssFailure2"
    ? "chrome://global/skin/icons/warning.svg"
    : "chrome://global/skin/icons/info.svg";

function getCSSClass() {
  return searchParams.get("s");
}

function getDescription() {
  return searchParams.get("d");
}

function isCaptive() {
  return searchParams.get("captive") == "true";
}

function retryThis(buttonEl) {
  RPMSendAsyncMessage("Browser:EnableOnlineMode");
  buttonEl.disabled = true;
}

function showBlockingErrorReporting() {
  // Display blocking error reporting UI for XFO error and CSP error.
  document.getElementById("blockingErrorReporting").hidden = false;
}

function showPrefChangeContainer() {
  const panel = document.getElementById("prefChangeContainer");
  panel.style.display = "block";
  document.getElementById("netErrorButtonContainer").style.display = "none";
  document
    .getElementById("prefResetButton")
    .addEventListener("click", function resetPreferences() {
      RPMSendAsyncMessage("Browser:ResetSSLPreferences");
    });
  setFocus("#prefResetButton", "beforeend");
}

function showTls10Container() {
  const panel = document.getElementById("enableTls10Container");
  panel.style.display = "block";
  document.getElementById("netErrorButtonContainer").style.display = "none";
  const button = document.getElementById("enableTls10Button");
  button.addEventListener("click", function enableTls10(e) {
    RPMSetBoolPref("security.tls.version.enable-deprecated", true);
    retryThis(button);
  });
  setFocus("#enableTls10Button", "beforeend");
}

function toggleCertErrorDebugInfoVisibility(shouldShow) {
  let debugInfo = document.getElementById("certificateErrorDebugInformation");
  let copyButton = document.getElementById("copyToClipboardTop");

  if (shouldShow === undefined) {
    shouldShow = debugInfo.hidden;
  }
  debugInfo.hidden = !shouldShow;
  if (shouldShow) {
    copyButton.scrollIntoView({ block: "start", behavior: "smooth" });
    copyButton.focus();
  }
}

function setupAdvancedButton() {
  // Get the hostname and add it to the panel
  var panel = document.getElementById("badCertAdvancedPanel");
  for (var span of panel.querySelectorAll("span.hostname")) {
    span.textContent = HOST_NAME;
  }

  // Register click handler for the weakCryptoAdvancedPanel
  document
    .getElementById("advancedButton")
    .addEventListener("click", togglePanelVisibility);

  function togglePanelVisibility() {
    panel.hidden = !panel.hidden;
    if (gIsCertError) {
      // Toggling the advanced panel must ensure that the debugging
      // information panel is hidden as well, since it's opened by the
      // error code link in the advanced panel.
      toggleCertErrorDebugInfoVisibility(false);
    }

    if (panel.style.display == "block") {
      // send event to trigger telemetry ping
      var event = new CustomEvent("AboutNetErrorUIExpanded", { bubbles: true });
      document.dispatchEvent(event);
    }
  }

  if (!gIsCertError) {
    return;
  }

  if (getCSSClass() == "expertBadCert") {
    panel.hidden = false;
  }

  disallowCertOverridesIfNeeded();
}

function disallowCertOverridesIfNeeded() {
  var cssClass = getCSSClass();
  // Disallow overrides if this is a Strict-Transport-Security
  // host and the cert is bad (STS Spec section 7.3) or if the
  // certerror is in a frame (bug 633691).
  if (cssClass == "badStsCert" || window != top) {
    document.getElementById("exceptionDialogButton").hidden = true;
  }
  if (cssClass == "badStsCert") {
    document.getElementById("badStsCertExplanation").removeAttribute("hidden");

    let stsReturnButtonText = document.getElementById("stsReturnButtonText")
      .textContent;
    document.getElementById("returnButton").textContent = stsReturnButtonText;
    document.getElementById(
      "advancedPanelReturnButton"
    ).textContent = stsReturnButtonText;

    let stsMitmWhatCanYouDoAboutIt3 = document.getElementById(
      "stsMitmWhatCanYouDoAboutIt3"
    ).innerHTML;
    // eslint-disable-next-line no-unsanitized/property
    document.getElementById(
      "mitmWhatCanYouDoAboutIt3"
    ).innerHTML = stsMitmWhatCanYouDoAboutIt3;
  }
}

function setErrorPageStrings(err) {
  let title = err + "-title";

  let isCertError = err == "nssBadCert";
  let className = getCSSClass();
  if (isCertError && (window !== window.top || className == "badStsCert")) {
    title = err + "-sts-title";
  }

  let cspXfoError = err === "cspBlocked" || err === "xfoBlocked";
  if (cspXfoError) {
    title = "csp-xfo-error-title";
  }

  let titleElement = document.querySelector(".title-text");

  if (!KNOWN_ERROR_TITLE_IDS.has(title)) {
    console.error("No strings exist for error:", title);
    title = "generic-title";
  }
  document.l10n.setAttributes(titleElement, title);
}

function initPage() {
  // We show an offline support page in case of a system-wide error,
  // when a user cannot connect to the internet and access the SUMO website.
  // For example, clock error, which causes certerrors across the web or
  // a security software conflict where the user is unable to connect
  // to the internet.
  // The URL that prompts us to show an offline support page should have the following
  // format: "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/supportPageSlug",
  // so we can extract the support page slug.
  let baseURL = RPMGetFormatURLPref("app.support.baseURL");
  let location = document.location.href;
  if (location.startsWith(baseURL)) {
    let supportPageSlug = document.location.pathname.split("/").pop();
    RPMSendAsyncMessage("DisplayOfflineSupportPage", {
      supportPageSlug,
    });
  }

  var err = gErrorCode;
  // List of error pages with an illustration.
  let illustratedErrors = [
    "malformedURI",
    "dnsNotFound",
    "connectionFailure",
    "netInterrupt",
    "netTimeout",
    "netReset",
    "netOffline",
  ];
  if (
    illustratedErrors.includes(err) &&
    !RPMGetBoolPref("browser.proton.enabled")
  ) {
    document.body.classList.add("illustrated", err);
  }
  if (err == "blockedByPolicy") {
    document.body.classList.add("blocked");
  }

  // Only worry about captive portals if this is a cert error.
  let showCaptivePortalUI = isCaptive() && gIsCertError;
  if (showCaptivePortalUI) {
    err = "captivePortal";
  }

  let l10nErrId = err;
  let className = getCSSClass();
  if (className) {
    document.body.classList.add(className);
  }

  if (gIsCertError && (window !== window.top || className == "badStsCert")) {
    l10nErrId += "_sts";
  }

  let pageTitle = document.getElementById("ept_" + l10nErrId);
  if (pageTitle) {
    document.title = pageTitle.textContent;
  }

  // if it's an unknown error or there's no title or description
  // defined, get the generic message
  var errDesc = document.getElementById("ed_" + l10nErrId);
  if (!errDesc) {
    errDesc = document.getElementById("ed_generic");
  }

  setErrorPageStrings(err);

  var sd = document.getElementById("errorShortDescText");
  if (sd) {
    if (gIsCertError) {
      // eslint-disable-next-line no-unsanitized/property
      sd.innerHTML = errDesc.innerHTML;
    } else {
      sd.textContent = getDescription();
    }
  }

  if (gIsCertError) {
    if (showCaptivePortalUI) {
      initPageCaptivePortal();
    } else {
      initPageCertError();
    }

    initCertErrorPageActions();
    setTechnicalDetailsOnCertError();
    return;
  }

  setFocus("#netErrorButtonContainer > .try-again");

  document.body.classList.add("neterror");

  var ld = document.getElementById("errorLongDesc");
  if (ld) {
    // eslint-disable-next-line no-unsanitized/property
    ld.innerHTML = errDesc.innerHTML;
  }

  if (err == "sslv3Used") {
    document.getElementById("learnMoreContainer").style.display = "block";
    document.body.className = "certerror";
  }

  // remove undisplayed errors to avoid bug 39098
  var errContainer = document.getElementById("errorContainer");
  errContainer.remove();

  let learnMoreLink = document.getElementById("learnMoreLink");
  learnMoreLink.setAttribute("href", baseURL + "connection-not-secure");

  if (err == "cspBlocked" || err == "xfoBlocked") {
    // Remove the "Try again" button for XFO and CSP violations,
    // since it's almost certainly useless. (Bug 553180)
    document.getElementById("netErrorButtonContainer").style.display = "none";

    // Adding a button for opening websites blocked for CSP and XFO violations
    // in a new window. (Bug 1461195)
    document.getElementById("errorShortDesc").style.display = "none";

    let hostString = document.location.hostname;
    let longDescription = document.getElementById("errorLongDesc");
    document.l10n.setAttributes(longDescription, "csp-xfo-blocked-long-desc", {
      hostname: hostString,
    });

    document.getElementById("openInNewWindowContainer").style.display = "block";

    let openInNewWindowButton = document.getElementById(
      "openInNewWindowButton"
    );
    openInNewWindowButton.href = document.location.href;

    // Add a learn more link
    document.getElementById("learnMoreContainer").style.display = "block";
    learnMoreLink.setAttribute("href", baseURL + "xframe-neterror-page");

    setupBlockingReportingUI();
  }

  setNetErrorMessageFromCode();

  // Pinning errors are of type nssFailure2
  if (err == "nssFailure2") {
    document.getElementById("learnMoreContainer").style.display = "block";

    const errorCode = document.getNetErrorInfo().errorCodeString;
    const isTlsVersionError =
      errorCode == "SSL_ERROR_UNSUPPORTED_VERSION" ||
      errorCode == "SSL_ERROR_PROTOCOL_VERSION_ALERT";
    const tls10OverrideEnabled = RPMGetBoolPref(
      "security.tls.version.enable-deprecated"
    );

    if (
      isTlsVersionError &&
      !tls10OverrideEnabled &&
      !RPMPrefIsLocked("security.tls.version.min")
    ) {
      // security.tls.* prefs may be reset by the user when they
      // encounter an error, so it's important that this has a
      // different pref branch.
      const showOverride = RPMGetBoolPref(
        "security.certerrors.tls.version.show-override",
        true
      );

      // This is probably a TLS 1.0 server; offer to re-enable.
      if (showOverride) {
        showTls10Container();
      }
    } else {
      const hasPrefStyleError = [
        "interrupted", // This happens with subresources that are above the max tls
        "SSL_ERROR_NO_CIPHERS_SUPPORTED",
        "SSL_ERROR_NO_CYPHER_OVERLAP",
        "SSL_ERROR_PROTOCOL_VERSION_ALERT",
        "SSL_ERROR_SSL_DISABLED",
        "SSL_ERROR_UNSUPPORTED_VERSION",
      ].some(substring => {
        return substring == errorCode;
      });

      if (hasPrefStyleError) {
        RPMAddMessageListener("HasChangedCertPrefs", msg => {
          if (msg.data.hasChangedCertPrefs) {
            // Configuration overrides might have caused this; offer to reset.
            showPrefChangeContainer();
          }
        });
        RPMSendAsyncMessage("GetChangedCertPrefs");
      }
    }
  }

  if (err == "sslv3Used") {
    document.getElementById("advancedButton").style.display = "none";
  }

  if (err == "inadequateSecurityError" || err == "blockedByPolicy") {
    // Remove the "Try again" button from pages that don't need it.
    // For HTTP/2 inadequate security or pages blocked by policy, trying
    // again won't help.
    document.getElementById("netErrorButtonContainer").style.display = "none";

    var container = document.getElementById("errorLongDesc");
    for (var span of container.querySelectorAll("span.hostname")) {
      span.textContent = HOST_NAME;
    }
  }
}

function setupBlockingReportingUI() {
  let checkbox = document.getElementById("automaticallyReportBlockingInFuture");

  let reportingAutomatic = RPMGetBoolPref(
    "security.xfocsp.errorReporting.automatic"
  );
  checkbox.checked = !!reportingAutomatic;

  checkbox.addEventListener("change", function({ target: { checked } }) {
    onSetBlockingReportAutomatic(checked);
  });

  let reportingEnabled = RPMGetBoolPref(
    "security.xfocsp.errorReporting.enabled"
  );

  if (!reportingEnabled) {
    return;
  }

  showBlockingErrorReporting();

  if (reportingAutomatic) {
    reportBlockingError();
  }
}

function reportBlockingError() {
  // We only report if we are in a frame.
  if (window === window.top) {
    return;
  }

  let err = gErrorCode;
  // Ensure we only deal with XFO and CSP here.
  if (!["xfoBlocked", "cspBlocked"].includes(err)) {
    return;
  }

  let xfo_header = RPMGetHttpResponseHeader("X-Frame-Options");
  let csp_header = RPMGetHttpResponseHeader("Content-Security-Policy");

  // Extract the 'CSP: frame-ancestors' from the CSP header.
  let reg = /(?:^|\s)frame-ancestors\s([^;]*)[$]*/i;
  let match = reg.exec(csp_header);
  csp_header = match ? match[1] : "";

  // If it's the csp error page without the CSP: frame-ancestors, this means
  // this error page is not triggered by CSP: frame-ancestors. So, we bail out
  // early.
  if (err === "cspBlocked" && !csp_header) {
    return;
  }

  let xfoAndCspInfo = {
    error_type: err === "xfoBlocked" ? "xfo" : "csp",
    xfo_header,
    csp_header,
  };

  // Trimming the tail colon symbol.
  let scheme = document.location.protocol.slice(0, -1);

  RPMSendAsyncMessage("ReportBlockingError", {
    scheme,
    host: document.location.host,
    port: parseInt(document.location.port) || -1,
    path: document.location.pathname,
    xfoAndCspInfo,
  });
}

function onSetBlockingReportAutomatic(checked) {
  RPMSetBoolPref("security.xfocsp.errorReporting.automatic", checked);

  // If we're enabling reports, send a report for this failure.
  if (checked) {
    reportBlockingError();
  }
}

async function setNetErrorMessageFromCode() {
  let hostString = HOST_NAME;

  let port = document.location.port;
  if (port && port != 443) {
    hostString += ":" + port;
  }

  let securityInfo;
  try {
    securityInfo = document.getNetErrorInfo();
  } catch (ex) {
    // We don't have a securityInfo when this is for example a DNS error.
    return;
  }

  let desc = document.getElementById("errorShortDescText");
  let errorCodeStr = securityInfo.errorCodeString || "";
  let errorCodeStrId = errorCodeStr
    .split("_")
    .join("-")
    .toLowerCase();
  let errorCodeMsg = "";

  if (KNOWN_ERROR_MESSAGE_IDS.has(errorCodeStrId)) {
    [errorCodeMsg] = await document.l10n.formatValues([errorCodeStrId]);
  }

  if (!errorCodeMsg) {
    console.warn("This error page has no error code in its security info");
    document.l10n.setAttributes(desc, "ssl-connection-error", {
      errorMessage: errorCodeStr,
      hostname: hostString,
    });
    return;
  }

  document.l10n.setAttributes(desc, "ssl-connection-error", {
    errorMessage: errorCodeMsg,
    hostname: hostString,
  });

  let desc2 = document.getElementById("errorShortDescText2");
  document.l10n.setAttributes(desc2, "cert-error-code-prefix", {
    error: errorCodeStr,
  });
}

function initPageCaptivePortal() {
  document.body.className = "captiveportal";
  document
    .getElementById("openPortalLoginPageButton")
    .addEventListener("click", () => {
      RPMSendAsyncMessage("Browser:OpenCaptivePortalPage");
    });

  setFocus("#openPortalLoginPageButton");
  setupAdvancedButton();

  // When the portal is freed, an event is sent by the parent process
  // that we can pick up and attempt to reload the original page.
  RPMAddMessageListener("AboutNetErrorCaptivePortalFreed", () => {
    document.location.reload();
  });
}

function initPageCertError() {
  document.body.classList.add("certerror");
  for (let host of document.querySelectorAll(".hostname")) {
    host.textContent = HOST_NAME;
  }

  setFocus("#returnButton");
  setupAdvancedButton();
  document.getElementById("learnMoreContainer").style.display = "block";

  let hideAddExceptionButton = RPMGetBoolPref(
    "security.certerror.hideAddException",
    false
  );
  if (hideAddExceptionButton) {
    document.querySelector(".exceptionDialogButtonContainer").hidden = true;
  }

  let els = document.querySelectorAll("[data-telemetry-id]");
  for (let el of els) {
    el.addEventListener("click", recordClickTelemetry);
  }

  let failedCertInfo = document.getFailedCertSecurityInfo();
  // Truncate the error code to avoid going over the allowed
  // string size limit for telemetry events.
  let errorCode = failedCertInfo.errorCodeString.substring(0, 40);
  RPMRecordTelemetryEvent(
    "security.ui.certerror",
    "load",
    "aboutcerterror",
    errorCode,
    {
      has_sts: (getCSSClass() == "badStsCert").toString(),
      is_frame: (window.parent != window).toString(),
    }
  );

  setCertErrorDetails();
}

function recordClickTelemetry(e) {
  let target = e.originalTarget;
  let telemetryId = target.dataset.telemetryId;
  let failedCertInfo = document.getFailedCertSecurityInfo();
  // Truncate the error code to avoid going over the allowed
  // string size limit for telemetry events.
  let errorCode = failedCertInfo.errorCodeString.substring(0, 40);
  RPMRecordTelemetryEvent(
    "security.ui.certerror",
    "click",
    telemetryId,
    errorCode,
    {
      has_sts: (getCSSClass() == "badStsCert").toString(),
      is_frame: (window.parent != window).toString(),
    }
  );
}

function initCertErrorPageActions() {
  document
    .getElementById("returnButton")
    .addEventListener("click", onReturnButtonClick);
  document
    .getElementById("advancedPanelReturnButton")
    .addEventListener("click", onReturnButtonClick);
  document
    .getElementById("copyToClipboardTop")
    .addEventListener("click", copyPEMToClipboard);
  document
    .getElementById("copyToClipboardBottom")
    .addEventListener("click", copyPEMToClipboard);
  document
    .getElementById("exceptionDialogButton")
    .addEventListener("click", addCertException);
}

function addCertException() {
  const isPermanent =
    !RPMIsWindowPrivate() &&
    RPMGetBoolPref("security.certerrors.permanentOverride");
  document.addCertException(!isPermanent).then(
    () => {
      location.reload();
    },
    err => {}
  );
}

function onReturnButtonClick(e) {
  RPMSendAsyncMessage("Browser:SSLErrorGoBack");
}

async function copyPEMToClipboard(e) {
  let details = await getFailedCertificatesAsPEMString();
  navigator.clipboard.writeText(details);
}

async function getFailedCertificatesAsPEMString() {
  let location = document.location.href;
  let failedCertInfo = document.getFailedCertSecurityInfo();
  let errorMessage = failedCertInfo.errorMessage;
  let hasHSTS = failedCertInfo.hasHSTS.toString();
  let hasHPKP = failedCertInfo.hasHPKP.toString();
  let [
    hstsLabel,
    hpkpLabel,
    failedChainLabel,
  ] = await document.l10n.formatValues([
    { id: "cert-error-details-hsts-label", args: { hasHSTS } },
    { id: "cert-error-details-key-pinning-label", args: { hasHPKP } },
    { id: "cert-error-details-cert-chain-label" },
  ]);

  let certStrings = failedCertInfo.certChainStrings;
  let failedChainCertificates = "";
  for (let der64 of certStrings) {
    let wrapped = der64.replace(/(\S{64}(?!$))/g, "$1\r\n");
    failedChainCertificates +=
      "-----BEGIN CERTIFICATE-----\r\n" +
      wrapped +
      "\r\n-----END CERTIFICATE-----\r\n";
  }

  let details =
    location +
    "\r\n\r\n" +
    errorMessage +
    "\r\n\r\n" +
    hstsLabel +
    "\r\n" +
    hpkpLabel +
    "\r\n\r\n" +
    failedChainLabel +
    "\r\n\r\n" +
    failedChainCertificates;
  return details;
}

function setCertErrorDetails(event) {
  // Check if the connection is being man-in-the-middled. When the parent
  // detects an intercepted connection, the page may be reloaded with a new
  // error code (MOZILLA_PKIX_ERROR_MITM_DETECTED).
  let failedCertInfo = document.getFailedCertSecurityInfo();
  let mitmPrimingEnabled = RPMGetBoolPref(
    "security.certerrors.mitm.priming.enabled"
  );
  if (
    mitmPrimingEnabled &&
    failedCertInfo.errorCodeString == "SEC_ERROR_UNKNOWN_ISSUER" &&
    // Only do this check for top-level failures.
    window.parent == window
  ) {
    RPMSendAsyncMessage("Browser:PrimeMitm");
  }

  let learnMoreLink = document.getElementById("learnMoreLink");
  let baseURL = RPMGetFormatURLPref("app.support.baseURL");
  learnMoreLink.setAttribute("href", baseURL + "connection-not-secure");
  let errWhatToDo = document.getElementById(
    "es_nssBadCert_" + failedCertInfo.errorCodeString
  );
  let es = document.getElementById("errorWhatToDoText");
  let errWhatToDoTitle = document.getElementById("edd_nssBadCert");
  let est = document.getElementById("errorWhatToDoTitleText");
  let error = gErrorCode;

  if (error == "sslv3Used") {
    learnMoreLink.setAttribute("href", baseURL + "sslv3-error-messages");
  }

  if (error == "nssFailure2") {
    let shortDesc = document.getElementById("errorShortDescText").textContent;
    // nssFailure2 also gets us other non-overrideable errors. Choose
    // a "learn more" link based on description:
    if (shortDesc.includes("MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE")) {
      learnMoreLink.setAttribute(
        "href",
        baseURL + "certificate-pinning-reports"
      );
    }
  }

  // This is set to true later if the user's system clock is at fault for this error.
  let clockSkew = false;
  document.body.setAttribute("code", failedCertInfo.errorCodeString);

  let titleElement = document.querySelector(".title-text");
  let desc;
  switch (failedCertInfo.errorCodeString) {
    case "SSL_ERROR_BAD_CERT_DOMAIN":
    case "SEC_ERROR_OCSP_INVALID_SIGNING_CERT":
    case "SEC_ERROR_UNKNOWN_ISSUER":
      if (es) {
        // eslint-disable-next-line no-unsanitized/property
        es.innerHTML = errWhatToDo.innerHTML;
      }
      if (est) {
        // eslint-disable-next-line no-unsanitized/property
        est.innerHTML = errWhatToDoTitle.innerHTML;
      }
      break;

    // This error code currently only exists for the Symantec distrust
    // in Firefox 63, so we add copy explaining that to the user.
    // In case of future distrusts of that scale we might need to add
    // additional parameters that allow us to identify the affected party
    // without replicating the complex logic from certverifier code.
    case "MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED":
      desc = document.getElementById("errorShortDescText2");
      document.l10n.setAttributes(
        desc,
        "cert-error-symantec-distrust-description",
        {
          HOST_NAME,
        }
      );

      let adminDesc = document.createElement("p");
      document.l10n.setAttributes(
        adminDesc,
        "cert-error-symantec-distrust-admin"
      );

      learnMoreLink.href = baseURL + "symantec-warning";
      break;

    case "MOZILLA_PKIX_ERROR_MITM_DETECTED":
      let autoEnabledEnterpriseRoots = RPMGetBoolPref(
        "security.enterprise_roots.auto-enabled",
        false
      );
      if (mitmPrimingEnabled && autoEnabledEnterpriseRoots) {
        RPMSendAsyncMessage("Browser:ResetEnterpriseRootsPref");
      }

      // We don't actually know what the MitM is called (since we don't
      // maintain a list), so we'll try and display the common name of the
      // root issuer to the user. In the worst case they are as clueless as
      // before, in the best case this gives them an actionable hint.
      // This may be revised in the future.
      let names = document.querySelectorAll(".mitm-name");
      for (let span of names) {
        span.textContent = failedCertInfo.issuerCommonName;
      }

      learnMoreLink.href = baseURL + "security-error";

      document.l10n.setAttributes(titleElement, "certerror-mitm-title");
      desc = document.getElementById("ed_mitm");
      // eslint-disable-next-line no-unsanitized/property
      document.getElementById("errorShortDescText").innerHTML = desc.innerHTML;

      // eslint-disable-next-line no-unsanitized/property
      es.innerHTML = errWhatToDo.innerHTML;
      // eslint-disable-next-line no-unsanitized/property
      est.innerHTML = errWhatToDoTitle.innerHTML;
      break;

    case "MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT":
      learnMoreLink.href = baseURL + "security-error";
      break;

    // In case the certificate expired we make sure the system clock
    // matches the remote-settings service (blocklist via Kinto) ping time
    // and is not before the build date.
    case "SEC_ERROR_EXPIRED_CERTIFICATE":
    case "SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE":
    case "MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE":
    case "MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE":
      learnMoreLink.href = baseURL + "time-errors";
      // We check against the remote-settings server time first if available, because that allows us
      // to give the user an approximation of what the correct time is.
      let difference = RPMGetIntPref("services.settings.clock_skew_seconds", 0);
      let lastFetched =
        RPMGetIntPref("services.settings.last_update_seconds", 0) * 1000;

      let now = Date.now();
      let certRange = {
        notBefore: failedCertInfo.certValidityRangeNotBefore,
        notAfter: failedCertInfo.certValidityRangeNotAfter,
      };
      let approximateDate = now - difference * 1000;
      // If the difference is more than a day, we last fetched the date in the last 5 days,
      // and adjusting the date per the interval would make the cert valid, warn the user:
      if (
        Math.abs(difference) > 60 * 60 * 24 &&
        now - lastFetched <= 60 * 60 * 24 * 5 * 1000 &&
        certRange.notBefore < approximateDate &&
        certRange.notAfter > approximateDate
      ) {
        clockSkew = true;
        // If there is no clock skew with Kinto servers, check against the build date.
        // (The Kinto ping could have happened when the time was still right, or not at all)
      } else {
        let appBuildID = RPMGetAppBuildID();
        let year = parseInt(appBuildID.substr(0, 4), 10);
        let month = parseInt(appBuildID.substr(4, 2), 10) - 1;
        let day = parseInt(appBuildID.substr(6, 2), 10);

        let buildDate = new Date(year, month, day);
        let systemDate = new Date();

        // We don't check the notBefore of the cert with the build date,
        // as it is of course almost certain that it is now later than the build date,
        // so we shouldn't exclude the possibility that the cert has become valid
        // since the build date.
        if (
          buildDate > systemDate &&
          new Date(certRange.notAfter) > buildDate
        ) {
          clockSkew = true;
        }
      }

      let systemDate = formatter.format(new Date());
      document.getElementById(
        "wrongSystemTime_systemDate1"
      ).textContent = systemDate;
      if (clockSkew) {
        document.body.classList.add("illustrated", "clockSkewError");
        document.l10n.setAttributes(titleElement, "clockSkewError-title");
        let clockErrDesc = document.getElementById("ed_clockSkewError");
        desc = document.getElementById("errorShortDescText");
        document.getElementById("errorShortDesc").style.display = "block";
        if (desc) {
          // eslint-disable-next-line no-unsanitized/property
          desc.innerHTML = clockErrDesc.innerHTML;
        }
        let errorPageContainer = document.getElementById("errorPageContainer");
        let textContainer = document.getElementById("text-container");
        errorPageContainer.style.backgroundPosition = `left top calc(50vh - ${textContainer.clientHeight /
          2}px)`;
      } else {
        let targetElems = document.querySelectorAll(
          "#wrongSystemTime_systemDate2"
        );
        for (let elem of targetElems) {
          elem.textContent = systemDate;
        }

        let errDesc = document.getElementById(
          "ed_nssBadCert_SEC_ERROR_EXPIRED_CERTIFICATE"
        );
        let sd = document.getElementById("errorShortDescText");
        // eslint-disable-next-line no-unsanitized/property
        sd.innerHTML = errDesc.innerHTML;

        let span = sd.querySelector(".hostname");
        span.textContent = HOST_NAME;

        // The secondary description mentions expired certificates explicitly
        // and should only be shown if the certificate has actually expired
        // instead of being not yet valid.
        if (failedCertInfo.errorCodeString == "SEC_ERROR_EXPIRED_CERTIFICATE") {
          let cssClass = getCSSClass();
          let stsSuffix = cssClass == "badStsCert" ? "_sts" : "";
          let errDesc2 = document.getElementById(
            `ed2_nssBadCert_SEC_ERROR_EXPIRED_CERTIFICATE${stsSuffix}`
          );
          let sd2 = document.getElementById("errorShortDescText2");
          // eslint-disable-next-line no-unsanitized/property
          sd2.innerHTML = errDesc2.innerHTML;
          if (
            Math.abs(difference) <= 60 * 60 * 24 &&
            now - lastFetched <= 60 * 60 * 24 * 5 * 1000
          ) {
            errWhatToDo = document.getElementById(
              "es_nssBadCert_SSL_ERROR_BAD_CERT_DOMAIN"
            );
          }
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
      }
      break;
  }
}

// The optional argument is only here for testing purposes.
async function setTechnicalDetailsOnCertError(
  failedCertInfo = document.getFailedCertSecurityInfo()
) {
  let technicalInfo = document.getElementById("badCertTechnicalInfo");

  function setL10NLabel(l10nId, args = {}, attrs = {}, rewrite = true) {
    let elem = document.createElement("label");
    if (rewrite) {
      technicalInfo.textContent = "";
    }
    technicalInfo.appendChild(elem);

    let newLines = document.createTextNode("\n \n");
    technicalInfo.appendChild(newLines);

    if (attrs) {
      let link = document.createElement("a");
      for (let attr of Object.keys(attrs)) {
        link.setAttribute(attr, attrs[attr]);
      }
      elem.appendChild(link);
    }

    if (args) {
      document.l10n.setAttributes(elem, l10nId, args);
    } else {
      document.l10n.setAttributes(elem, l10nId);
    }
  }

  let cssClass = getCSSClass();
  let error = gErrorCode;

  let hostString = HOST_NAME;
  let port = document.location.port;
  if (port && port != 443) {
    hostString += ":" + port;
  }

  let l10nId;
  let args = {
    hostname: hostString,
  };
  if (failedCertInfo.isUntrusted) {
    switch (failedCertInfo.errorCodeString) {
      case "MOZILLA_PKIX_ERROR_MITM_DETECTED":
        setL10NLabel("cert-error-mitm-intro");
        setL10NLabel("cert-error-mitm-mozilla", {}, {}, false);
        setL10NLabel("cert-error-mitm-connection", {}, {}, false);
        break;
      case "SEC_ERROR_UNKNOWN_ISSUER":
        setL10NLabel("cert-error-trust-unknown-issuer-intro");
        setL10NLabel("cert-error-trust-unknown-issuer", args, {}, false);
        break;
      case "SEC_ERROR_CA_CERT_INVALID":
        setL10NLabel("cert-error-intro", args);
        setL10NLabel("cert-error-trust-cert-invalid", {}, {}, false);
        break;
      case "SEC_ERROR_UNTRUSTED_ISSUER":
        setL10NLabel("cert-error-intro", args);
        setL10NLabel("cert-error-trust-untrusted-issuer", {}, {}, false);
        break;
      case "SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED":
        setL10NLabel("cert-error-intro", args);
        setL10NLabel(
          "cert-error-trust-signature-algorithm-disabled",
          {},
          {},
          false
        );
        break;
      case "SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE":
        setL10NLabel("cert-error-intro", args);
        setL10NLabel("cert-error-trust-expired-issuer", {}, {}, false);
        break;
      case "MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT":
        setL10NLabel("cert-error-intro", args);
        setL10NLabel("cert-error-trust-self-signed", {}, {}, false);
        break;
      case "MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED":
        setL10NLabel("cert-error-intro", args);
        setL10NLabel("cert-error-trust-symantec", {}, {}, false);
        break;
      default:
        setL10NLabel("cert-error-intro", args);
        setL10NLabel("cert-error-untrusted-default", {}, {}, false);
    }
  } else if (failedCertInfo.isDomainMismatch) {
    let serverCertBase64 = failedCertInfo.certChainStrings[0];
    let parsed = await parse(pemToDER(serverCertBase64));
    let subjectAltNamesExtension = parsed.ext.san;
    let subjectAltNames = [];
    if (subjectAltNamesExtension) {
      for (let name of subjectAltNamesExtension.altNames) {
        if (name[0] == "DNS Name" && name[1].length) {
          subjectAltNames.push(name[1]);
        }
      }
    }
    let numSubjectAltNames = subjectAltNames.length;
    if (numSubjectAltNames != 0) {
      if (numSubjectAltNames == 1) {
        args["alt-name"] = subjectAltNames[0];

        // Let's check if we want to make this a link.
        let okHost = subjectAltNames[0];
        let href = "";
        let thisHost = HOST_NAME;
        let proto = document.location.protocol + "//";
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
          document.getElementById("badCertAdvancedPanel").style.display =
            "block";
          if (error == "nssBadCert") {
            // Toggling the advanced panel must ensure that the debugging
            // information panel is hidden as well, since it's opened by the
            // error code link in the advanced panel.
            toggleCertErrorDebugInfoVisibility(false);
          }
        }

        // Set the link if we want it.
        if (href) {
          setL10NLabel("cert-error-domain-mismatch-single", args, {
            href,
            "data-l10n-name": "domain-mismatch-link",
            id: "cert_domain_link",
          });
        } else {
          setL10NLabel("cert-error-domain-mismatch-single-nolink", args);
        }
      } else {
        let names = subjectAltNames.join(", ");
        args["subject-alt-names"] = names;
        setL10NLabel("cert-error-domain-mismatch-multiple", args);
      }
    } else {
      setL10NLabel("cert-error-domain-mismatch", { hostname: hostString });
    }
  } else if (failedCertInfo.isNotValidAtThisTime) {
    let notBefore = failedCertInfo.validNotBefore;
    let notAfter = failedCertInfo.validNotAfter;
    args = {
      hostname: hostString,
    };
    if (notBefore && Date.now() < notAfter) {
      let notBeforeLocalTime = formatter.format(new Date(notBefore));
      l10nId = "cert-error-not-yet-valid-now";
      args["not-before-local-time"] = notBeforeLocalTime;
    } else {
      let notAfterLocalTime = formatter.format(new Date(notAfter));
      l10nId = "cert-error-expired-now";
      args["not-after-local-time"] = notAfterLocalTime;
    }
    setL10NLabel(l10nId, args);
  }

  setL10NLabel(
    "cert-error-code-prefix-link",
    { error: failedCertInfo.errorCodeString },
    {
      title: failedCertInfo.errorCodeString,
      id: "errorCode",
      "data-l10n-name": "error-code-link",
      "data-telemetry-id": "error_code_link",
      href: "#certificateErrorDebugInformation",
    },
    false
  );
  let errorCodeLink = document.getElementById("errorCode");
  if (errorCodeLink) {
    // We're attaching the event listener to the parent element and not on
    // the errorCodeLink itself because event listeners cannot be attached
    // to fluent DOM overlays.
    technicalInfo.addEventListener("click", handleErrorCodeClick);
  }

  let div = document.getElementById("certificateErrorText");
  div.textContent = await getFailedCertificatesAsPEMString();
}

function handleErrorCodeClick(event) {
  if (event.target.id !== "errorCode") {
    return;
  }
  event.preventDefault();
  toggleCertErrorDebugInfoVisibility();
  recordClickTelemetry(event);
}

/* Only focus if we're the toplevel frame; otherwise we
   don't want to call attention to ourselves!
*/
function setFocus(selector, position = "afterbegin") {
  if (window.top == window) {
    var button = document.querySelector(selector);
    var parent = button.parentNode;
    parent.insertAdjacentElement(position, button);
    // It's possible setFocus was called via the DOMContentLoaded event
    // handler and that the button has no frame. Things without a frame cannot
    // be focused. We use a requestAnimationFrame to queue up the focus to occur
    // once the button has its frame.
    requestAnimationFrame(() => {
      button.focus({ preventFocusRing: true });
    });
  }
}

for (let button of document.querySelectorAll(".try-again")) {
  button.addEventListener("click", function() {
    retryThis(this);
  });
}

window.addEventListener("DOMContentLoaded", () => {
  // Expose this so tests can call it.
  window.setTechnicalDetailsOnCertError = setTechnicalDetailsOnCertError;

  initPage();
  // Dispatch this event so tests can detect that we finished loading the error page.
  let event = new CustomEvent("AboutNetErrorLoad", { bubbles: true });
  document.dispatchEvent(event);
});
