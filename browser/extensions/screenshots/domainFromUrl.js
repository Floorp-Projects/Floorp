/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/** Returns the domain of a URL, but safely and in ASCII; URLs without domains
    (such as about:blank) return the scheme, Unicode domains get stripped down
    to ASCII */

"use strict";

this.domainFromUrl = (function() {
  return function urlDomainForId(location) {
    // eslint-disable-line no-unused-vars
    let domain = location.hostname;
    if (!domain) {
      domain = location.origin.split(":")[0];
      if (!domain) {
        domain = "unknown";
      }
    }
    if (domain.search(/^[a-z0-9._-]{1,1000}$/i) === -1) {
      // Probably a unicode domain; we could use punycode but it wouldn't decode
      // well in the URL anyway.  Instead we'll punt.
      domain = domain.replace(/[^a-z0-9._-]/gi, "");
      if (!domain) {
        domain = "site";
      }
    }
    return domain;
  };
})();
null;
