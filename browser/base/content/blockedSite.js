// Error url MUST be formatted like this:
//   about:blocked?e=error_code&u=url(&o=1)?
//     (o=1 when user overrides are allowed)

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

function getURL() {
  var url = document.documentURI;
  var match = url.match(/&u=([^&]+)&/);

  // match == null if not found; if so, return an empty string
  // instead of what would turn out to be portions of the URI
  if (!match)
    return "";

  url = decodeURIComponent(match[1]);

  // If this is a view-source page, then get then real URI of the page
  if (url.startsWith("view-source:"))
    url = url.slice(12);
  return url;
}

/**
 * Check whether this warning page is overridable or not, in which case
 * the "ignore the risk" suggestion in the error description
 * should not be shown.
 */
function getOverride() {
  var url = document.documentURI;
  var match = url.match(/&o=1&/);
  return !!match;
}

/**
 * Attempt to get the hostname via document.location.  Fail back
 * to getURL so that we always return something meaningful.
 */
function getHostString() {
  try {
    return document.location.hostname;
  } catch (e) {
    return getURL();
  }
}

function onClickSeeDetails() {
  let details = document.getElementById("errorDescriptionContainer");
  if (details.hidden) {
    details.removeAttribute("hidden");
  } else {
    details.setAttribute("hidden", "true");
  }
}

function initPage() {
  var error = "";
  switch (getErrorCode()) {
    case "malwareBlocked" :
      error = "malware";
      break;
    case "deceptiveBlocked" :
      error = "phishing";
      break;
    case "unwantedBlocked" :
      error = "unwanted";
      break;
    case "harmfulBlocked" :
      error = "harmful";
      break;
    default:
      return;
  }

  var el;

  if (error !== "malware") {
    el = document.getElementById("errorTitleText_malware");
    el.remove();
    el = document.getElementById("errorShortDescText_malware");
    el.remove();
    el = document.getElementById("errorLongDesc_malware");
    el.remove();
  }

  if (error !== "phishing") {
    el = document.getElementById("errorTitleText_phishing");
    el.remove();
    el = document.getElementById("errorShortDescText_phishing");
    el.remove();
    el = document.getElementById("errorLongDesc_phishing");
    el.remove();
  }

  if (error !== "unwanted") {
    el = document.getElementById("errorTitleText_unwanted");
    el.remove();
    el = document.getElementById("errorShortDescText_unwanted");
    el.remove();
    el = document.getElementById("errorLongDesc_unwanted");
    el.remove();
  }

  if (error !== "harmful") {
    el = document.getElementById("errorTitleText_harmful");
    el.remove();
    el = document.getElementById("errorShortDescText_harmful");
    el.remove();
    el = document.getElementById("errorLongDesc_harmful");
    el.remove();
  }

  // Decide which version of the string should be visible in the error description.
  if (getOverride()) {
    document.getElementById(error + "_error_desc_no_override").remove();
  } else {
    document.getElementById(error + "_error_desc_override").remove();
  }

  // Set sitename in error details.
  let sitenameElem = document.getElementById(error + "_sitename");
  sitenameElem.setAttribute("class", "sitename");
  sitenameElem.textContent = getHostString();

  document.title = document.getElementById("errorTitleText_" + error).textContent;

  // Inform the test harness that we're done loading the page.
  var event = new CustomEvent("AboutBlockedLoaded",
    {
      bubbles: true,
      detail: {
        url: this.getURL(),
        err: error
      }
    });
  document.dispatchEvent(event);
}

let seeDetailsButton = document.getElementById("seeDetailsButton");
seeDetailsButton.addEventListener("click", onClickSeeDetails);
// Note: It is important to run the script this way, instead of using
// an onload handler. This is because error pages are loaded as
// LOAD_BACKGROUND, which means that onload handlers will not be executed.
initPage();
