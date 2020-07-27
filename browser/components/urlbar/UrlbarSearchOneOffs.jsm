/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarSearchOneOffs"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  SearchOneOffs: "resource:///modules/SearchOneOffs.jsm",
});

/**
 * The one-off search buttons in the urlbar.
 */
class UrlbarSearchOneOffs extends SearchOneOffs {}
