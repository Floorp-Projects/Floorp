/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Error url MUST be formatted like this:
//   moz-neterror:page?e=error&u=url&d=desc
//
// or optionally, to specify an alternate CSS class to allow for
// custom styling and favicon:
//
//   moz-neterror:page?e=error&u=url&s=classname&d=desc

// Note that this file uses document.documentURI to get
// the URL (with the format from above). This is because
// document.location.href gets the current URI off the docshell,
// which is the URL displayed in the location bar, i.e.
// the URI that the user attempted to load.

function getErrorCode() {
  var url = document.documentURI;
  var error = url.search(/e\=/);
  var duffUrl = url.search(/\&u\=/);
  return decodeURIComponent(url.slice(error + 2, duffUrl));
}

function getCSSClass() {
  var url = document.documentURI;
  var matches = url.match(/s\=([^&]+)\&/);
  // s is optional, if no match just return nothing
  if (!matches || matches.length < 2) {
    return "";
  }

  // parenthetical match is the second entry
  return decodeURIComponent(matches[1]);
}

function getDescription() {
  var url = document.documentURI;
  var desc = url.search(/d\=/);

  // desc == -1 if not found; if so, return an empty string
  // instead of what would turn out to be portions of the URI
  if (desc == -1) {
    return "";
  }

  return decodeURIComponent(url.slice(desc + 2));
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

function initPage() {
  var err = getErrorCode();

  // if it's an unknown error or there's no title or description
  // defined, get the generic message
  var errTitle = document.getElementById("et_" + err);
  var errDesc = document.getElementById("ed_" + err);
  if (!errTitle || !errDesc) {
    errTitle = document.getElementById("et_generic");
    errDesc = document.getElementById("ed_generic");
  }

  var title = document.getElementById("errorTitleText");
  if (title) {
    title.parentNode.replaceChild(errTitle, title);
    // change id to the replaced child's id so styling works
    errTitle.id = "errorTitleText";
  }

  var sd = document.getElementById("errorShortDescText");
  if (sd) {
    sd.textContent = getDescription();
  }

  var ld = document.getElementById("errorLongDesc");
  if (ld) {
    ld.parentNode.replaceChild(errDesc, ld);
    // change id to the replaced child's id so styling works
    errDesc.id = "errorLongDesc";
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
    favicon.setAttribute(
      "href",
      "chrome://global/skin/icons/" + className + "_favicon.png"
    );
    faviconParent.appendChild(favicon);
  }
  if (className == "expertBadCert") {
    showSecuritySection();
  }

  if (err == "remoteXUL") {
    // Remove the "Try again" button for remote XUL errors given that
    // it is useless.
    document.getElementById("errorTryAgain").style.display = "none";
  }

  if (err == "cspBlocked" || err == "xfoBlocked") {
    // Remove the "Try again" button for XFO and CSP violations, since it's
    // almost certainly useless. (Bug 553180)
    document.getElementById("errorTryAgain").style.display = "none";
  }

  if (err == "nssBadCert") {
    // Remove the "Try again" button for security exceptions, since it's
    // almost certainly useless.
    document.getElementById("errorTryAgain").style.display = "none";
    document
      .getElementById("errorPageContainer")
      .setAttribute("class", "certerror");
    addDomainErrorLink();
  } else {
    // Remove the override block for non-certificate errors.  CSS-hiding
    // isn't good enough here, because of bug 39098
    var secOverride = document.getElementById("securityOverrideDiv");
    secOverride.remove();
  }

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

  if (document.getElementById("errorTryAgain").style.display != "none") {
    addAutofocus("errorTryAgain");
  }
}

function showSecuritySection() {
  // Swap link out, content in
  document.getElementById("securityOverrideContent").style.display = "";
  document.getElementById("securityOverrideLink").style.display = "none";
}

/* In the case of SSL error pages about domain mismatch, see if
 * we can hyperlink the user to the correct site.  We don't want
 * to do this generically since it allows MitM attacks to redirect
 * users to a site under attacker control, but in certain cases
 * it is safe (and helpful!) to do so.  Bug 402210 */
function addDomainErrorLink() {
  // Rather than textContent, we need to treat description as HTML
  var sd = document.getElementById("errorShortDescText");
  if (sd) {
    var desc = getDescription();

    // sanitize description text - see bug 441169

    // First, find the index of the <a> tag we care about, being careful not to
    // use an over-greedy regex
    var re = /<a id="cert_domain_link" title="([^"]+)">/;
    var result = re.exec(desc);
    if (!result) {
      return;
    }

    // Remove sd's existing children
    sd.textContent = "";

    // Everything up to the link should be text content
    sd.appendChild(document.createTextNode(desc.slice(0, result.index)));

    // Now create the link itself
    var anchorEl = document.createElement("a");
    anchorEl.setAttribute("id", "cert_domain_link");
    anchorEl.setAttribute("title", result[1]);
    anchorEl.appendChild(document.createTextNode(result[1]));
    sd.appendChild(anchorEl);

    // Finally, append text for anything after the closing </a>
    sd.appendChild(
      document.createTextNode(desc.slice(desc.indexOf("</a>") + "</a>".length))
    );
  }

  var link = document.getElementById("cert_domain_link");
  if (!link) {
    return;
  }

  var okHost = link.getAttribute("title");
  var thisHost = document.location.hostname;
  var proto = document.location.protocol;

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
  if (endsWith(okHost, "." + thisHost)) {
    link.href = proto + okHost;
  }

  /* case #2:
   * browser.garage.maemo.org uses an invalid security certificate.
   *
   * The certificate is only valid for garage.maemo.org
   */
  if (endsWith(thisHost, "." + okHost)) {
    link.href = proto + okHost;
  }
}

function endsWith(haystack, needle) {
  return haystack.slice(-needle.length) == needle;
}

/* Only do autofocus if we're the toplevel frame; otherwise we
 * don't want to call attention to ourselves!  The key part is
 * that autofocus happens on insertion into the tree, so we
 * can remove the button, add @autofocus, and reinsert the
 * button. */
function addAutofocus(buttonId) {
  if (window.top == window) {
    let button = document.getElementById(buttonId);
    let nextSibling = button.nextSibling;
    let parent = button.parentNode;
    button.remove();
    button.setAttribute("autofocus", "true");
    parent.insertBefore(button, nextSibling);
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
