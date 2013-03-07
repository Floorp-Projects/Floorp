/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Loader } = require('sdk/test/loader');

const { loader } = LoaderWithHookedConsole(module);

const pb = loader.require('sdk/private-browsing');
const pbUtils = loader.require('sdk/private-browsing/utils');

function LoaderWithHookedConsole(module) {
  let globals = {};
  let errors = [];

  globals.console = Object.create(console, {
    error: {
      value: function(e) {
        errors.push(e);
        if (!/DEPRECATED:/.test(e)) {
          console.error(e);
        }
      }
    }
  });

  let loader = Loader(module, globals);

  return {
    loader: loader,
    errors: errors
  }
}

function deactivate(callback) {
  if (pbUtils.isGlobalPBSupported) {
    if (callback)
      pb.once('stop', callback);
    pb.deactivate();
  }
}
exports.deactivate = deactivate;

exports.pb = pb;
exports.pbUtils = pbUtils;
exports.LoaderWithHookedConsole = LoaderWithHookedConsole;
