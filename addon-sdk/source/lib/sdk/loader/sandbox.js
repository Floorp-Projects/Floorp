/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental"
};

const { Cc, Ci, CC, Cu } = require('chrome');
const systemPrincipal = CC('@mozilla.org/systemprincipal;1', 'nsIPrincipal')();
const scriptLoader = Cc['@mozilla.org/moz/jssubscript-loader;1'].
                     getService(Ci.mozIJSSubScriptLoader);

/**
 * Make a new sandbox that inherits given `source`'s principals. Source can be
 * URI string, DOMWindow or `null` for system principals.
 */
function sandbox(target, options) {
  return Cu.Sandbox(target || systemPrincipal, options || {});
}
exports.sandbox = sandbox

/**
 * Evaluates given `source` in a given `sandbox` and returns result.
 */
function evaluate(sandbox, code, uri, line, version) {
  return Cu.evalInSandbox(code, sandbox, version || '1.8', uri || '', line || 1);
}
exports.evaluate = evaluate;

/**
 * Evaluates code under the given `uri` in the given `sandbox`.
 *
 * @param {String} uri
 *    The URL pointing to the script to load.
 *    It must be a local chrome:, resource:, file: or data: URL.
 */
function load(sandbox, uri) {
  if (uri.indexOf('data:') === 0) {
    let source = uri.substr(uri.indexOf(',') + 1);

    return evaluate(sandbox, decodeURIComponent(source), '1.8', uri, 0);
  } else {
    return scriptLoader.loadSubScript(uri, sandbox, 'UTF-8');
  }
}
exports.load = load;
