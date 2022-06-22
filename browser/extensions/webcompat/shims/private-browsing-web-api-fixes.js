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

delete window.wrappedJSObject.caches;
delete window.wrappedJSObject.indexedDB;
