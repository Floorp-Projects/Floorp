/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { utils: Cu } = Components;

const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});
const { BrowserLoader } = Cu.import("resource://devtools/client/shared/browser-loader.js", {});

// Module Loader
const require = BrowserLoader({
  baseURI: "resource://devtools/client/dom/",
  window: this
}).require;

XPCOMUtils.defineConstant(this, "require", require);

// Localization
const { LocalizationHelper } = require("devtools/shared/l10n");
this.l10n = new LocalizationHelper("devtools/locale/dom.properties");

// Load DOM panel content
require("./content/dom-view.js");
