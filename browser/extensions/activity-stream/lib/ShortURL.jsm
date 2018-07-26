ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "IDNService", "@mozilla.org/network/idn-service;1", "nsIIDNService");

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

/**
 * Properly convert internationalized domain names.
 * @param {string} host Domain hostname.
 * @returns {string} Hostname suitable to be displayed.
 */
function handleIDNHost(hostname) {
  try {
    return IDNService.convertToDisplayIDN(hostname, {});
  } catch (e) {
    // If something goes wrong (e.g. host is an IP address) just fail back
    // to the full domain.
    return hostname;
  }
}

/**
 * Get the effective top level domain of a host.
 * @param {string} host The host to be analyzed.
 * @return {str} The suffix or empty string if there's no suffix.
 */
function getETLD(host) {
  try {
    return Services.eTLD.getPublicSuffixFromHost(host);
  } catch (err) {
    return "";
  }
}

/**
 * shortURL - Creates a short version of a link's url, used for display purposes
 *            e.g. {url: http://www.foosite.com}  =>  "foosite"
 *
 * @param  {obj} link A link object
 *         {str} link.url (required)- The url of the link
 * @return {str}   A short url
 */
function shortURL({url}) {
  if (!url) {
    return "";
  }

  // Make sure we have a valid / parseable url
  let parsed;
  try {
    parsed = new URL(url);
  } catch (ex) {
    // Not entirely sure what we have, but just give it back
    return url;
  }

  // Clean up the url (lowercase hostname via URL and remove www.)
  const hostname = parsed.hostname.replace(/^www\./i, "");

  // Remove the eTLD (e.g., com, net) and the preceding period from the hostname
  const eTLD = getETLD(hostname);
  const eTLDExtra = eTLD.length > 0 ? -(eTLD.length + 1) : Infinity;

  // Ideally get the short eTLD-less host but fall back to longer url parts
  return handleIDNHost(hostname.slice(0, eTLDExtra) || hostname) ||
    parsed.pathname || parsed.href;
}

const EXPORTED_SYMBOLS = ["shortURL", "getETLD"];
