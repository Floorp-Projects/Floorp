Components.utils.importGlobalProperties(["URL"]);

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
  const {eTLD} = link;
  const hostname = (link.hostname || new URL(link.url).hostname).replace(/^www\./i, "");

  // Remove the eTLD (e.g., com, net) and the preceding period from the hostname
  const eTLDLength = (eTLD || "").length || (hostname.match(/\.com$/) && 3);
  const eTLDExtra = eTLDLength > 0 ? -(eTLDLength + 1) : Infinity;
  // If URL and hostname are not present fallback to page title.
  return hostname.slice(0, eTLDExtra).toLowerCase() || hostname || link.title || link.url;
};

this.EXPORTED_SYMBOLS = ["shortURL"];
