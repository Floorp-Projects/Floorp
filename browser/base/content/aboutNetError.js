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

// Set to true on init if the error code is nssBadCert.
let gIsCertError;

function getErrorCode() {
  return searchParams.get("e");
}

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
  // Note: The application may wish to handle switching off "offline mode"
  // before this event handler runs, but using a capturing event handler.

  // Session history has the URL of the page that failed
  // to load, not the one of the error page. So, just call
  // reload(), which will also repost POST data correctly.
  try {
    location.reload();
  } catch (e) {
    // We probably tried to reload a URI that caused an exception to
    // occur;  e.g. a nonexistent file.
  }

  buttonEl.disabled = true;
}

function toggleDisplay(node) {
  const toggle = {
    "": "block",
    "none": "block",
    "block": "none"
  };
  return (node.style.display = toggle[node.style.display]);
}

function showCertificateErrorReporting() {
  // Display error reporting UI
  document.getElementById("certificateErrorReporting").style.display = "block";
}

function showPrefChangeContainer() {
  const panel = document.getElementById("prefChangeContainer");
  panel.style.display = "block";
  document.getElementById("netErrorButtonContainer").style.display = "none";
  document.getElementById("prefResetButton").addEventListener("click", function resetPreferences(e) {
    const event = new CustomEvent("AboutNetErrorResetPreferences", {bubbles: true});
    document.dispatchEvent(event);
  });
  addAutofocus("prefResetButton", "beforeend");
}

function setupAdvancedButton() {
  // Get the hostname and add it to the panel
  var panel = document.getElementById("badCertAdvancedPanel");
  for (var span of panel.querySelectorAll("span.hostname")) {
    span.textContent = document.location.hostname;
  }

  // Register click handler for the weakCryptoAdvancedPanel
  document.getElementById("advancedButton")
          .addEventListener("click", function togglePanelVisibility() {
    toggleDisplay(panel);
    if (gIsCertError) {
      // Toggling the advanced panel must ensure that the debugging
      // information panel is hidden as well, since it's opened by the
      // error code link in the advanced panel.
      var div = document.getElementById("certificateErrorDebugInformation");
      div.style.display = "none";
    }

    if (panel.style.display == "block") {
      // send event to trigger telemetry ping
      var event = new CustomEvent("AboutNetErrorUIExpanded", {bubbles: true});
      document.dispatchEvent(event);
    }
  });

  if (!gIsCertError) {
    return;
  }

  if (getCSSClass() == "expertBadCert") {
    toggleDisplay(document.getElementById("badCertAdvancedPanel"));
    // Toggling the advanced panel must ensure that the debugging
    // information panel is hidden as well, since it's opened by the
    // error code link in the advanced panel.
    var div = document.getElementById("certificateErrorDebugInformation");
    div.style.display = "none";
  }

  disallowCertOverridesIfNeeded();
}

function disallowCertOverridesIfNeeded() {
  var cssClass = getCSSClass();
  // Disallow overrides if this is a Strict-Transport-Security
  // host and the cert is bad (STS Spec section 7.3) or if the
  // certerror is in a frame (bug 633691).
  if (cssClass == "badStsCert" || window != top) {
    document.getElementById("exceptionDialogButton").setAttribute("hidden", "true");
  }
  if (cssClass == "badStsCert") {
    document.getElementById("badStsCertExplanation").removeAttribute("hidden");
  }
}

function initPage() {
  var err = getErrorCode();
  // List of error pages with an illustration.
  let illustratedErrors = [
    "malformedURI", "dnsNotFound", "connectionFailure", "netInterrupt",
    "netTimeout", "netReset", "netOffline",
  ];
  if (illustratedErrors.includes(err)) {
    document.body.classList.add("illustrated", err);
  }
  if (err == "blockedByPolicy") {
    document.body.classList.add("blocked");
  }

  gIsCertError = (err == "nssBadCert");
  // Only worry about captive portals if this is a cert error.
  let showCaptivePortalUI = isCaptive() && gIsCertError;
  if (showCaptivePortalUI) {
    err = "captivePortal";
  }

  let pageTitle = document.getElementById("ept_" + err);
  if (pageTitle) {
    document.title = pageTitle.textContent;
  }

  // if it's an unknown error or there's no title or description
  // defined, get the generic message
  var errTitle = document.getElementById("et_" + err);
  var errDesc  = document.getElementById("ed_" + err);
  if (!errTitle || !errDesc) {
    errTitle = document.getElementById("et_generic");
    errDesc  = document.getElementById("ed_generic");
  }

  // eslint-disable-next-line no-unsanitized/property
  document.querySelector(".title-text").innerHTML = errTitle.innerHTML;

  var sd = document.getElementById("errorShortDescText");
  if (sd) {
    if (gIsCertError) {
      // eslint-disable-next-line no-unsanitized/property
      sd.innerHTML = errDesc.innerHTML;
    } else {
      sd.textContent = getDescription();
    }
  }
  if (showCaptivePortalUI) {
    initPageCaptivePortal();
    return;
  }
  if (gIsCertError) {
    initPageCertError();
    updateContainerPosition();
    return;
  }
  addAutofocus("errorTryAgain");

  document.body.classList.add("neterror");

  var ld = document.getElementById("errorLongDesc");
  if (ld) {
    // eslint-disable-next-line no-unsanitized/property
    ld.innerHTML = errDesc.innerHTML;
  }

  if (err == "sslv3Used") {
    document.getElementById("learnMoreContainer").style.display = "block";
    let learnMoreLink = document.getElementById("learnMoreLink");
    learnMoreLink.href = "https://support.mozilla.org/kb/how-resolve-sslv3-error-messages-firefox";
    document.body.className = "certerror";
  }

  // remove undisplayed errors to avoid bug 39098
  var errContainer = document.getElementById("errorContainer");
  errContainer.remove();

  var className = getCSSClass();
  if (className && className != "expertBadCert") {
    // Associate a CSS class with the root of the page, if one was passed in,
    // to allow custom styling.
    // Not "expertBadCert" though, don't want to deal with the favicon
    document.documentElement.className = className;

    // Also, if they specified a CSS class, they must supply their own
    // favicon.  In order to trigger the browser to repaint though, we
    // need to remove/add the link element.
    var favicon = document.getElementById("favicon");
    var faviconParent = favicon.parentNode;
    faviconParent.removeChild(favicon);
    favicon.setAttribute("href", "chrome://global/skin/icons/" + className + "_favicon.png");
    faviconParent.appendChild(favicon);
  }

  if (err == "remoteXUL") {
    // Remove the "Try again" button for remote XUL errors given that
    // it is useless.
    document.getElementById("netErrorButtonContainer").style.display = "none";
  }

  if (err == "cspBlocked") {
    // Remove the "Try again" button for CSP violations, since it's
    // almost certainly useless. (Bug 553180)
    document.getElementById("netErrorButtonContainer").style.display = "none";
  }

  window.addEventListener("AboutNetErrorOptions", function(evt) {
    // Pinning errors are of type nssFailure2
    if (getErrorCode() == "nssFailure2") {
      let shortDesc = document.getElementById("errorShortDescText").textContent;
      document.getElementById("learnMoreContainer").style.display = "block";
      let learnMoreLink = document.getElementById("learnMoreLink");
      // nssFailure2 also gets us other non-overrideable errors. Choose
      // a "learn more" link based on description:
      if (shortDesc.includes("MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE")) {
        learnMoreLink.href = "https://support.mozilla.org/kb/certificate-pinning-reports";
      }

      var options = JSON.parse(evt.detail);
      if (options && options.enabled) {
        var checkbox = document.getElementById("automaticallyReportInFuture");
        showCertificateErrorReporting();
        if (options.automatic) {
          // set the checkbox
          checkbox.checked = true;
        }

        checkbox.addEventListener("change", function(changeEvt) {
            var event = new CustomEvent("AboutNetErrorSetAutomatic",
              {bubbles: true,
               detail: changeEvt.target.checked});
            document.dispatchEvent(event);
          });
      }
      const hasPrefStyleError = [
        "interrupted", // This happens with subresources that are above the max tls
        "SSL_ERROR_PROTOCOL_VERSION_ALERT",
        "SSL_ERROR_UNSUPPORTED_VERSION",
        "SSL_ERROR_NO_CYPHER_OVERLAP",
        "SSL_ERROR_NO_CIPHERS_SUPPORTED"
      ].some((substring) => shortDesc.includes(substring));
      // If it looks like an error that is user config based
      if (getErrorCode() == "nssFailure2" && hasPrefStyleError && options && options.changedCertPrefs) {
        showPrefChangeContainer();
      }
    }
    if (getErrorCode() == "sslv3Used") {
      document.getElementById("advancedButton").style.display = "none";
    }
  }, true, true);

  var event = new CustomEvent("AboutNetErrorLoad", {bubbles: true});
  document.dispatchEvent(event);

  if (err == "inadequateSecurityError" || err == "blockedByPolicy") {
    // Remove the "Try again" button from pages that don't need it.
    // For HTTP/2 inadequate security or pages blocked by policy, trying
    // again won't help.
    document.getElementById("errorTryAgain").style.display = "none";

    var container = document.getElementById("errorLongDesc");
    for (var span of container.querySelectorAll("span.hostname")) {
      span.textContent = document.location.hostname;
    }
  }
}

function updateContainerPosition() {
  let textContainer = document.getElementById("text-container");
  textContainer.style.marginTop = `calc(50vh - ${textContainer.clientHeight / 2}px)`;
}

function initPageCaptivePortal() {
  document.body.className = "captiveportal";
  document.getElementById("openPortalLoginPageButton")
          .addEventListener("click", () => {
    let event = new CustomEvent("AboutNetErrorOpenCaptivePortal", {bubbles: true});
    document.dispatchEvent(event);
  });

  addAutofocus("openPortalLoginPageButton");
  setupAdvancedButton();

  // When the portal is freed, an event is generated by the frame script
  // that we can pick up and attempt to reload the original page.
  window.addEventListener("AboutNetErrorCaptivePortalFreed", () => {
    document.location.reload();
  });
}

function initPageCertError() {
  document.body.className = "certerror";
  for (let host of document.querySelectorAll(".hostname")) {
    host.textContent = document.location.hostname;
  }

  addAutofocus("returnButton");
  setupAdvancedButton();

  document.getElementById("learnMoreContainer").style.display = "block";

  let checkbox = document.getElementById("automaticallyReportInFuture");
  checkbox.addEventListener("change", function({target: {checked}}) {
    document.dispatchEvent(new CustomEvent("AboutNetErrorSetAutomatic", {
      detail: checked,
      bubbles: true
    }));
  });

  addEventListener("AboutNetErrorOptions", function(event) {
    var options = JSON.parse(event.detail);
    if (options && options.enabled) {
      // Display error reporting UI
      document.getElementById("certificateErrorReporting").style.display = "block";

      // set the checkbox
      checkbox.checked = !!options.automatic;
    }
    if (options && options.hideAddExceptionButton) {
      document.querySelector(".exceptionDialogButtonContainer").hidden = true;
    }
  }, true, true);

  let event = new CustomEvent("AboutNetErrorLoad", {bubbles: true});
  document.getElementById("advancedButton").dispatchEvent(event);
}

/* Only do autofocus if we're the toplevel frame; otherwise we
   don't want to call attention to ourselves!  The key part is
   that autofocus happens on insertion into the tree, so we
   can remove the button, add @autofocus, and reinsert the
   button.
*/
function addAutofocus(buttonId, position = "afterbegin") {
  if (window.top == window) {
      var button = document.getElementById(buttonId);
      var parent = button.parentNode;
      button.remove();
      button.setAttribute("autofocus", "true");
      parent.insertAdjacentElement(position, button);
  }
}

let errorTryAgain = document.getElementById("errorTryAgain");
errorTryAgain.addEventListener("click", function() {
  retryThis(this);
});

// Note: It is important to run the script this way, instead of using
// an onload handler. This is because error pages are loaded as
// LOAD_BACKGROUND, which means that onload handlers will not be executed.
initPage();
