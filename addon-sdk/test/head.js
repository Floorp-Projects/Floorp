/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { utils: Cu } = Components;
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
const LoaderModule = Cu.import("resource://gre/modules/commonjs/toolkit/loader.js", {}).Loader;
const { console } = Cu.import("resource://gre/modules/devtools/Console.jsm", {});
let {
  Loader, main, Module, Require, unload
} = LoaderModule;

let CURRENT_DIR = gTestPath.replace(/\/[^\/]*\.js$/,'/');
let loaders = [];

// All tests are asynchronous.
waitForExplicitFinish();

let gEnableLogging = Services.prefs.getBoolPref("devtools.debugger.log");
Services.prefs.setBoolPref("devtools.debugger.log", true);

registerCleanupFunction(() => {
  info("finish() was called, cleaning up...");
  loaders.forEach(unload);
  Services.prefs.setBoolPref("devtools.debugger.log", gEnableLogging);
});

function makePaths (root) {
  return {
    './': CURRENT_DIR,
    '': 'resource://gre/modules/commonjs/'
  };
}

function makeLoader (options) {
  let { paths, globals } = options || {};

  // We have to have `console` as a global, otherwise
  // many SDK modules will fail
  // bug 961252
  let globalDefaults = {
    console: console
  };

  let loader = Loader({
    paths: paths || makePaths(),
    globals: extend({}, globalDefaults, globals) || null,
    modules: {
      // Needed because sdk/ modules reference utilities in
      // `toolkit/loader`, until Bug 961194 is completed
      'toolkit/loader': LoaderModule
    },
    // We need rootURI due to `sdk/self` (or are using native loader)
    // which overloads with pseudo modules
    // bug 961245
    rootURI: CURRENT_DIR,
    // We also need metadata dummy object
    // bug 961245
    metadata: {}
  });

  loaders.push(loader);
  return loader;
}

function isUUID (string) {
  return /^\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}$/.test(string);
}

function extend (...objs) {
  if (objs.length === 0 || objs.length === 1)
    return objs[0] || {};

  for (let i = objs.length; i > 1; i--) {
    for (var prop in objs[i - 1])
      objs[0][prop] = objs[i - 1][prop];
  }
  return objs[0];
}
