/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

let { Loader, main, unload, parseStack } = require('toolkit/loader');

let root = module.uri.substr(0, module.uri.lastIndexOf('/'))

// The following adds Debugger constructor to the global namespace.
const { Cu } = require('chrome');
const { addDebuggerToGlobal } = Cu.import('resource://gre/modules/jsdebugger.jsm', {});
addDebuggerToGlobal(this);

exports['test dependency cycles'] = function(assert) {
  let uri = root + '/fixtures/loader/cycles/';
  let loader = Loader({ paths: { '': uri } });

  let program = main(loader, 'main');

  assert.equal(program.a.b, program.b, 'module `a` gets correct `b`');
  assert.equal(program.b.a, program.a, 'module `b` gets correct `a`');
  assert.equal(program.c.main, program, 'module `c` gets correct `main`');

  unload(loader);
}

exports['test syntax errors'] = function(assert) {
  let uri = root + '/fixtures/loader/syntax-error/';
  let loader = Loader({ paths: { '': uri } });

  try {
    let program = main(loader, 'main');
  } catch (error) {
    assert.equal(error.name, "SyntaxError", "throws syntax error");
    assert.equal(error.fileName.split("/").pop(), "error.js",
              "Error contains filename");
    assert.equal(error.lineNumber, 11, "error is on line 11");
    let stack = parseStack(error.stack);

    assert.equal(stack.pop().fileName, uri + "error.js",
                 "last frame file containing syntax error");
    assert.equal(stack.pop().fileName, uri + "main.js",
                 "previous frame is a requirer module");
    assert.equal(stack.pop().fileName, module.uri,
                 "previous to it is a test module");

  } finally {
    unload(loader);
  }
}

exports['test missing module'] = function(assert) {
  let uri = root + '/fixtures/loader/missing/'
  let loader = Loader({ paths: { '': uri } });

  try {
    let program = main(loader, 'main')
  } catch (error) {
    assert.equal(error.message, "Module `not-found` is not found at " +
                uri + "not-found.js", "throws if error not found");

    assert.equal(error.fileName.split("/").pop(), "main.js",
                 "Error fileName is requirer module");

    assert.equal(error.lineNumber, 7, "error is on line 7");

    let stack = parseStack(error.stack);

    assert.equal(stack.pop().fileName, uri + "main.js",
                 "loader stack is omitted");

    assert.equal(stack.pop().fileName, module.uri,
                 "previous in the stack is test module");
  } finally {
    unload(loader);
  }
}

exports['test exceptions in modules'] = function(assert) {
  let uri = root + '/fixtures/loader/exceptions/'

  let loader = Loader({ paths: { '': uri } });

  try {
    let program = main(loader, 'main')
  } catch (error) {
    assert.equal(error.message, "Boom!", "thrown errors propagate");

    assert.equal(error.fileName.split("/").pop(), "boomer.js",
                 "Error comes from the module that threw it");

    assert.equal(error.lineNumber, 8, "error is on line 8");

    let stack = parseStack(error.stack);

    let frame = stack.pop()
    assert.equal(frame.fileName, uri + "boomer.js",
                 "module that threw is first in the stack");
    assert.equal(frame.name, "exports.boom",
                 "name is in the stack");

    frame = stack.pop()
    assert.equal(frame.fileName, uri + "main.js",
                 "module that called it is next in the stack");
    assert.equal(frame.lineNumber, 9, "caller line is in the stack");


    assert.equal(stack.pop().fileName, module.uri,
                 "this test module is next in the stack");
  } finally {
    unload(loader);
  }
}

exports['test early errors in module'] = function(assert) {
  let uri = root + '/fixtures/loader/errors/';
  let loader = Loader({ paths: { '': uri } });

  try {
    let program = main(loader, 'main')
  } catch (error) {
    assert.equal(String(error),
                 "Error: opening input stream (invalid filename?)",
                 "thrown errors propagate");

    assert.equal(error.fileName.split("/").pop(), "boomer.js",
                 "Error comes from the module that threw it");

    assert.equal(error.lineNumber, 7, "error is on line 7");

    let stack = parseStack(error.stack);

    let frame = stack.pop()
    assert.equal(frame.fileName, uri + "boomer.js",
                 "module that threw is first in the stack");

    frame = stack.pop()
    assert.equal(frame.fileName, uri + "main.js",
                 "module that called it is next in the stack");
    assert.equal(frame.lineNumber, 7, "caller line is in the stack");


    assert.equal(stack.pop().fileName, module.uri,
                 "this test module is next in the stack");
  } finally {
    unload(loader);
  }
};

exports['test require json'] = function (assert) {
  let data = require('./fixtures/loader/json/manifest.json');
  assert.equal(data.name, 'Jetpack Loader Test', 'loads json with strings');
  assert.equal(data.version, '1.0.1', 'loads json with strings');
  assert.equal(data.dependencies.async, '*', 'loads json with objects');
  assert.equal(data.dependencies.underscore, '*', 'loads json with objects');
  assert.equal(data.contributors.length, 4, 'loads json with arrays');
  assert.ok(Array.isArray(data.contributors), 'loads json with arrays');
  data.version = '2.0.0';
  let newdata = require('./fixtures/loader/json/manifest.json');
  assert.equal(newdata.version, '2.0.0',
    'JSON objects returned should be cached and the same instance');

  try {
    require('./fixtures/loader/json/invalid.json');
    assert.fail('Error not thrown when loading invalid json');
  } catch (err) {
    assert.ok(err, 'error thrown when loading invalid json');
    assert.ok(/JSON\.parse/.test(err.message),
      'should thrown an error from JSON.parse, not attempt to load .json.js');
  }
 
  // Try again to ensure an empty module isn't loaded from cache
  try {
    require('./fixtures/loader/json/invalid.json');
    assert.fail('Error not thrown when loading invalid json a second time');
  } catch (err) {
    assert.ok(err,
      'error thrown when loading invalid json a second time');
    assert.ok(/JSON\.parse/.test(err.message),
      'should thrown an error from JSON.parse a second time, not attempt to load .json.js');
  }
};

exports['test setting metadata for newly created sandboxes'] = function(assert) {
  let addonID = 'random-addon-id';
  let uri = root + '/fixtures/loader/cycles/';
  let loader = Loader({ paths: { '': uri }, id: addonID });

  let dbg = new Debugger();
  dbg.onNewGlobalObject = function(global) {
    dbg.onNewGlobalObject = undefined;

    let metadata = Cu.getSandboxMetadata(global.unsafeDereference());
    assert.ok(metadata, 'this global has attached metadata');
    assert.equal(metadata.URI, uri + 'main.js', 'URI is set properly');
    assert.equal(metadata.addonID, addonID, 'addon ID is set');
  }

  let program = main(loader, 'main');
};

exports['test require .json, .json.js'] = function (assert) {
  let testjson = require('./fixtures/loader/json/test.json');
  assert.equal(testjson.filename, 'test.json',
    'require("./x.json") should load x.json, not x.json.js');

  let nodotjson = require('./fixtures/loader/json/nodotjson.json');
  assert.equal(nodotjson.filename, 'nodotjson.json.js',
    'require("./x.json") should load x.json.js when x.json does not exist');
  nodotjson.data.prop = 'hydralisk';

  // require('nodotjson.json') and require('nodotjson.json.js')
  // should resolve to the same file
  let nodotjsonjs = require('./fixtures/loader/json/nodotjson.json.js');
  assert.equal(nodotjsonjs.data.prop, 'hydralisk',
    'js modules are cached whether access via .json.js or .json');
};

require('test').run(exports);
