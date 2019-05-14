/* eslint-env mozilla/frame-script */

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
    "block": "none",
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
  addAutofocus("#prefResetButton", "beforeend");
}

function setupAdvancedButton() {
  // Get the hostname and add it to the panel
  var panel = document.getElementById("badCertAdvancedPanel");
  for (var span of panel.querySelectorAll("span.hostname")) {
    span.textContent = document.location.hostname;
  }

  // Register click handler for the weakCryptoAdvancedPanel
  document.getElementById("advancedButton").addEventListener("click", togglePanelVisibility);

  function togglePanelVisibility() {
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
  }

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

    let stsReturnButtonText = document.getElementById("stsReturnButtonText").textContent;
    document.getElementById("returnButton").textContent = stsReturnButtonText;
    document.getElementById("advancedPanelReturnButton").textContent = stsReturnButtonText;

    let stsMitmWhatCanYouDoAboutIt3 =
      document.getElementById("stsMitmWhatCanYouDoAboutIt3").innerHTML;
    // eslint-disable-next-line no-unsanitized/property
    document.getElementById("mitmWhatCanYouDoAboutIt3").innerHTML = stsMitmWhatCanYouDoAboutIt3;
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
  var errTitle = document.getElementById("et_" + l10nErrId);
  var errDesc  = document.getElementById("ed_" + l10nErrId);
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
  addAutofocus("#netErrorButtonContainer > .try-again");

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
        "SSL_ERROR_NO_CIPHERS_SUPPORTED",
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
    document.getElementById("netErrorButtonContainer").style.display = "none";

    var container = document.getElementById("errorLongDesc");
    for (var span of container.querySelectorAll("span.hostname")) {
      span.textContent = document.location.hostname;
    }
  }
}

// This function centers the error container after its content updates.
// It is currently duplicated in NetErrorChild.jsm to avoid having to do
// async communication to the page that would result in flicker.
// TODO(johannh): Get rid of this duplication.
function updateContainerPosition() {
  let textContainer = document.getElementById("text-container");
  // Using the vh CSS property our margin adapts nicely to window size changes.
  // Unfortunately, this doesn't work correctly in iframes, which is why we need
  // to manually compute the height there.
  if (window.parent == window) {
    textContainer.style.marginTop = `calc(50vh - ${textContainer.clientHeight / 2}px)`;
  } else {
    let offset = (document.documentElement.clientHeight / 2) - (textContainer.clientHeight / 2);
    if (offset > 0) {
      textContainer.style.marginTop = `${offset}px`;
    }
  }
}

function initPageCaptivePortal() {
  document.body.className = "captiveportal";
  document.getElementById("openPortalLoginPageButton")
          .addEventListener("click", () => {
    RPMSendAsyncMessage("Browser:OpenCaptivePortalPage");
  });

  addAutofocus("#openPortalLoginPageButton");
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
    host.textContent = document.location.hostname;
  }

  addAutofocus("#returnButton");
  setupAdvancedButton();

  document.getElementById("learnMoreContainer").style.display = "block";

  let checkbox = document.getElementById("automaticallyReportInFuture");
  checkbox.addEventListener("change", function({target: {checked}}) {
    document.dispatchEvent(new CustomEvent("AboutNetErrorSetAutomatic", {
      detail: checked,
      bubbles: true,
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
function addAutofocus(selector, position = "afterbegin") {
  if (window.top == window) {
      var button = document.querySelector(selector);
      var parent = button.parentNode;
      button.remove();
      button.setAttribute("autofocus", "true");
      parent.insertAdjacentElement(position, button);
  }
}

for (let button of document.querySelectorAll(".try-again")) {
  button.addEventListener("click", function() {
    retryThis(this);
  });
}

// Note: It is important to run the script this way, instead of using
// an onload handler. This is because error pages are loaded as
// LOAD_BACKGROUND, which means that onload handlers will not be executed.
initPage();
