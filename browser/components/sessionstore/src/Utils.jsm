/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Utils"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm", this);

this.Utils = Object.freeze({
  makeURI: function (url) {
    return Services.io.newURI(url, null, null);
  },

  /**
   * Returns true if the |url| passed in is part of the given root |domain|.
   * For example, if |url| is "www.mozilla.org", and we pass in |domain| as
   * "mozilla.org", this will return true. It would return false the other way
   * around.
   */
  hasRootDomain: function (url, domain) {
    let host;

    try {
      host = this.makeURI(url).host;
    } catch (e) {
      // The given URL probably doesn't have a host.
      return false;
    }

    let index = host.indexOf(domain);
    if (index == -1)
      return false;

    if (host == domain)
      return true;

    let prevChar = host[index - 1];
    return (index == (host.length - domain.length)) &&
           (prevChar == "." || prevChar == "/");
  },

  shallowCopy: function (obj) {
    let retval = {};

    for (let key of Object.keys(obj)) {
      retval[key] = obj[key];
    }

    return retval;
  }
});
