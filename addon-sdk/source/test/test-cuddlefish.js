/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Cc, Ci, Cu, CC, Cr, Cm, ChromeWorker, components } = require("chrome");

const packaging = require("@loader/options");
const app = require('sdk/system/xul-app');
const { resolve } = require;

const scriptLoader = Cc['@mozilla.org/moz/jssubscript-loader;1'].
                     getService(Ci.mozIJSSubScriptLoader);
const systemPrincipal = CC('@mozilla.org/systemprincipal;1', 'nsIPrincipal')();

function loadSandbox(uri) {
  let proto = {
    sandboxPrototype: {
      loadSandbox: loadSandbox,
      ChromeWorker: ChromeWorker
    }
  };
  let sandbox = Cu.Sandbox(systemPrincipal, proto);
  // Create a fake commonjs environnement just to enable loading loader.js
  // correctly
  sandbox.exports = {};
  sandbox.module = { uri: uri, exports: sandbox.exports };
  sandbox.require = function (id) {
    if (id !== "chrome")
      throw new Error("Bootstrap sandbox `require` method isn't implemented.");

    return Object.freeze({ Cc: Cc, Ci: Ci, Cu: Cu, Cr: Cr, Cm: Cm,
      CC: CC, components: components,
      ChromeWorker: ChromeWorker });
  };
  scriptLoader.loadSubScript(uri, sandbox, 'UTF-8');
  return sandbox;
}

exports['test loader'] = function(assert) {
  let { Loader, Require, unload, override } = loadSandbox(resolve('sdk/loader/cuddlefish.js')).exports;
  var prints = [];
  function print(message) {
    prints.push(message);
  }

  let loader = Loader(override(packaging, {
    globals: {
      print: print,
      foo: 1
    }
  }));
  let require = Require(loader, module);

  var fixture = require('./loader/fixture');

  assert.equal(fixture.foo, 1, 'custom globals must work.');
  assert.equal(fixture.bar, 2, 'exports are set');

  assert.equal(prints[0], 'testing', 'global print must be injected.');

  var unloadsCalled = '';

  require("sdk/system/unload").when(function(reason) {
    assert.equal(reason, 'test', 'unload reason is passed');
    unloadsCalled += 'a';
  });
  require('sdk/system/unload.js').when(function() {
    unloadsCalled += 'b';
  });

  unload(loader, 'test');

  assert.equal(unloadsCalled, 'ba',
               'loader.unload() must call listeners in LIFO order.');
};

require('sdk/test').run(exports);
