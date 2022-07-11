/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/shared/loader/browser-loader.js"
);

// Module Loader
const require = BrowserLoader({
  baseURI: "resource://devtools/client/dom/",
  window,
}).require;

XPCOMUtils.defineConstant(this, "require", require);

// Localization
const { LocalizationHelper } = require("devtools/shared/l10n");
this.l10n = new LocalizationHelper("devtools/client/locales/dom.properties");

// Load DOM panel content
require("devtools/client/dom/content/dom-view.js");
