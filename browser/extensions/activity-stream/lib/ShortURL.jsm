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
 *            e.g. {url: http://www.foosite.com, eTLD: "com"}  =>  "foosite"
 *
 * @param  {obj} link A link object
 *         {str} link.url (required)- The url of the link
 *         {str} link.eTLD (required) - The tld of the link
 *               e.g. for https://foo.org, the tld would be "org"
 *               Note that this property is added in various queries for ActivityStream
 *               via Services.eTLD.getPublicSuffix
 *         {str} link.hostname (optional) - The hostname of the url
 *               e.g. for http://www.hello.com/foo/bar, the hostname would be "www.hello.com"
 *         {str} link.title (optional) - The title of the link
 * @return {str}   A short url
 */
this.shortURL = function shortURL(link) {
  if (!link.url && !link.hostname) {
    return "";
  }
  const eTLD = link.eTLD || getETLD(link.url);
  const hostname = (link.hostname || new URL(link.url).hostname).replace(/^www\./i, "");

  // Remove the eTLD (e.g., com, net) and the preceding period from the hostname
  const eTLDLength = (eTLD || "").length;
  const eTLDExtra = eTLDLength > 0 ? -(eTLDLength + 1) : Infinity;
  // If URL and hostname are not present fallback to page title.
  return handleIDNHost(hostname.slice(0, eTLDExtra).toLowerCase() || hostname) || link.title || link.url;
};

this.EXPORTED_SYMBOLS = ["shortURL", "getETLD"];
