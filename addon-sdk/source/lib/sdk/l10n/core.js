/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

lazyRequireModule(this, "./json/core", "json");
lazyRequireModule(this, "./properties/core", "properties");

const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyGetter(this, "get",
                            () => json.usingJSON ? json.get : properties.get);

module.exports = Object.freeze({
  get get() { return get; }, // ... yeah.
});
