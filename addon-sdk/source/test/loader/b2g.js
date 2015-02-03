/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu} = require("chrome");
const {readURISync} = require("sdk/net/url");

const systemPrincipal = Cc["@mozilla.org/systemprincipal;1"].
                        createInstance(Ci.nsIPrincipal);


const FakeCu = function() {
  const sandbox = Cu.Sandbox(systemPrincipal, {wantXrays: false});
  sandbox.toString = function() {
    return "[object BackstagePass]";
  }
  this.sandbox = sandbox;
}
FakeCu.prototype = {
  ["import"](url, scope) {
    const {sandbox} = this;
    sandbox.__URI__ = url;
    const target = Cu.createObjectIn(sandbox);
    target.toString = sandbox.toString;
    Cu.evalInSandbox(`(function(){` + readURISync(url) + `\n})`,
                     sandbox, "1.8", url).call(target);
    // Borrowed from mozJSComponentLoader.cpp to match errors closer.
    // https://github.com/mozilla/gecko-dev/blob/f6ca65e8672433b2ce1a0e7c31f72717930b5e27/js/xpconnect/loader/mozJSComponentLoader.cpp#L1205-L1208
    if (!Array.isArray(target.EXPORTED_SYMBOLS)) {
      throw Error("EXPORTED_SYMBOLS is not an array.");
    }

    for (let key of target.EXPORTED_SYMBOLS) {
      scope[key] = target[key];
    }

    return target;
  }
};
exports.FakeCu = FakeCu;
