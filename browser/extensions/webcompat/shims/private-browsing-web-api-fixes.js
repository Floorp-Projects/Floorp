/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1714354 - Fix for site issues with web APIs in private browsing
 *
 * Some sites expect specific DOM APIs to work in specific ways, which
 * is not always true, such as in private browsing mode. We work around
 * related breakage by undefining those APIs entirely in private
 * browsing mode for those sites.
 */

// caches.keys() rejects in private browsing mode:
// https://bugzilla.mozilla.org/show_bug.cgi?id=1742344#c4
// Can be removed once bug 1714354 is fixed.
delete window.wrappedJSObject.caches;
