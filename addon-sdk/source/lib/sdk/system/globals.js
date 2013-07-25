/* vim:set ts=2 sw=2 sts=2 expandtab */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

let { Cc, Ci, CC } = require('chrome');
let { PlainTextConsole } = require('../console/plain-text');
let { stdout } = require('../system');
let ScriptError = CC('@mozilla.org/scripterror;1', 'nsIScriptError');
let consoleService = Cc['@mozilla.org/consoleservice;1'].getService().
                     QueryInterface(Ci.nsIConsoleService);

// On windows dump does not writes into stdout so cfx can't read thous dumps.
// To workaround this issue we write to a special file from which cfx will
// read and print to the console.
// For more details see: bug-673383
exports.dump = stdout.write;

// Bug 718230: We need to send console messages to stdout and JS Console
function forsakenConsoleDump(msg, level) {
  stdout.write(msg);

  if (level === 'error') {
    let error = ScriptError();
    msg = msg.replace(/^error: /, '');
    error.init(msg, null, null, 0, 0, 0, 'Add-on SDK');
    consoleService.logMessage(error);
  }
  else
    consoleService.logStringMessage(msg);
};
exports.console = new PlainTextConsole(forsakenConsoleDump);

// Provide CommonJS `define` to allow authoring modules in a format that can be
// loaded both into jetpack and into browser via AMD loaders.
Object.defineProperty(exports, 'define', {
  // `define` is provided as a lazy getter that binds below defined `define`
  // function to the module scope, so that require, exports and module
  // variables remain accessible.
  configurable: true,
  get: function() {
    let sandbox = this;
    return function define(factory) {
      factory = Array.slice(arguments).pop();
      factory.call(sandbox, sandbox.require, sandbox.exports, sandbox.module);
    }
  }
});
