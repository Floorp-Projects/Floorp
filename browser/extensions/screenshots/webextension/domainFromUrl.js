/** Returns the domain of a URL, but safely and in ASCII; URLs without domains
    (such as about:blank) return the scheme, Unicode domains get stripped down
    to ASCII */

"use strict";

this.domainFromUrl = (function() {

  return function urlDomainForId(location) { // eslint-disable-line no-unused-vars
    let domain = location.hostname;
    if (!domain) {
      domain = location.origin.split(":")[0];
      if (!domain) {
        domain = "unknown";
      }
    }
    if (domain.search(/^[a-z0-9.\-]+$/i) === -1) {
      // Probably a unicode domain; we could use punycode but it wouldn't decode
      // well in the URL anyway.  Instead we'll punt.
      domain = domain.replace(/[^a-z0-9.\-]/ig, "");
      if (!domain) {
        domain = "site";
      }
    }
    return domain;
  };

})();
null;
