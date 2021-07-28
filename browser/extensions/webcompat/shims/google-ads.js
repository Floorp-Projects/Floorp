/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1713726 - Shim Ads by Google
 *
 * Sites relying on window.adsbygoogle may encounter breakage if it is blocked.
 * This shim provides a stub for that API to mitigate that breakage.
 */

if (window.adsbygoogle?.loaded === undefined) {
  window.adsbygoogle = {
    loaded: true,
    push() {},
  };
}
