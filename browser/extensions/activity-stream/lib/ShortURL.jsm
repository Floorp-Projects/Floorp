const {utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "IDNService", "@mozilla.org/network/idn-service;1", "nsIIDNService");

Cu.importGlobalProperties(["URL"]);

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
 * Returns the public suffix of a URL or empty string in case of error.
 * @param {string} url The url to be analyzed.
 */
function getETLD(url) {
  try {
    return Services.eTLD.getPublicSuffix(Services.io.newURI(url));
  } catch (err) {
    return "";
  }
}

this.getETLD = getETLD;

  /**
 * shortURL - Creates a short version of a link's url, used for display purposes
 *            e.g. {url: http://www.foosite.com}  =>  "foosite"
 *
 * @param  {obj} link A link object
 *         {str} link.url (required)- The url of the link
 *         {str} link.title (optional) - The title of the link
 * @return {str}   A short url
 */
this.shortURL = function shortURL(link) {
  if (!link.url) {
    return "";
  }

  // Remove the eTLD (e.g., com, net) and the preceding period from the hostname
  const eTLD = getETLD(link.url);
  const eTLDExtra = eTLD.length > 0 ? -(eTLD.length + 1) : Infinity;

  // Clean up the url and fallback to page title or url if necessary
  const hostname = (new URL(link.url).hostname).replace(/^www\./i, "");
  return handleIDNHost(hostname.slice(0, eTLDExtra).toLowerCase()) ||
    link.title || link.url;
};

this.EXPORTED_SYMBOLS = ["shortURL", "getETLD"];
