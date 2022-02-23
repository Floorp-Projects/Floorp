/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1754389 - Shim Maxmind GeoIP library
 *
 * Some sites rely on Maxmind's GeoIP library which gets blocked by ETP's
 * fingerprinter blocking. With the library window global not being defined
 * functionality may break or the site does not render at all. This shim adds a
 * dummy object which returns errors for any request to mitigate the breakage.
 */

if (!window.geoip2) {
  const callback = (_, onError) => {
    onError("");
  };

  window.geoip2 = {
    country: callback,
    city: callback,
    insights: callback,
  };
}
