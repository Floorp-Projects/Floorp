/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = function({ resource, targetFront }) {
  // When we are in the browser console we list indexedDBs internal to
  // Firefox e.g. defined inside a .jsm. Because there is no way before this
  // point to know whether or not we are inside the browser toolbox we have
  // already fetched the hostnames of these databases.
  //
  // If we are not inside the browser toolbox we need to delete these
  // hostnames.

  const SAFE_HOSTS_PREFIXES_REGEX = /^(about:|https?:|file:|moz-extension:)/;
  const { hosts } = resource;

  if (!targetFront.chrome) {
    const newHosts = {};

    for (const [host, dbs] of Object.entries(hosts)) {
      if (SAFE_HOSTS_PREFIXES_REGEX.test(host)) {
        newHosts[host] = dbs;
      }
    }

    resource.hosts = newHosts;
  }

  return resource;
};
