/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu, Cc, Ci } = require("chrome");
const { evaluate } = require("sdk/loader/sandbox");

const ROOT = require.resolve("sdk/base64").replace("/sdk/base64.js", "");

// Note: much of this test code is from
// http://dxr.mozilla.org/mozilla-central/source/toolkit/mozapps/extensions/internal/XPIProvider.jsm
const BOOTSTRAP_REASONS = {
  APP_STARTUP     : 1,
  APP_SHUTDOWN    : 2,
  ADDON_ENABLE    : 3,
  ADDON_DISABLE   : 4,
  ADDON_INSTALL   : 5,
  ADDON_UNINSTALL : 6,
  ADDON_UPGRADE   : 7,
  ADDON_DOWNGRADE : 8
};

function createBootstrapScope(options) {
  let { uri, id: aId } = options;
  let principal = Cc["@mozilla.org/systemprincipal;1"].
                  createInstance(Ci.nsIPrincipal);

  let bootstrapScope = new Cu.Sandbox(principal, {
    sandboxName: uri,
    wantGlobalProperties: ["indexedDB"],
    addonId: aId,
    metadata: { addonID: aId, URI: uri }
  });

  // Copy the reason values from the global object into the bootstrap scope.
  for (let name in BOOTSTRAP_REASONS)
    bootstrapScope[name] = BOOTSTRAP_REASONS[name];

  return bootstrapScope;
}
exports.create = createBootstrapScope;

function evaluateBootstrap(options) {
  let { uri, scope } = options;

  evaluate(scope,
    `${"Components"}.classes['@mozilla.org/moz/jssubscript-loader;1']
                    .createInstance(${"Components"}.interfaces.mozIJSSubScriptLoader)
                    .loadSubScript("${uri}");`, "ECMAv5");
}
exports.evaluate = evaluateBootstrap;
