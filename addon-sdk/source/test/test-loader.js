/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

let {
  Loader, main, unload, parseStack, generateMap, resolve, join
} = require('toolkit/loader');
let { readURI } = require('sdk/net/url');

let root = module.uri.substr(0, module.uri.lastIndexOf('/'))


// The following adds Debugger constructor to the global namespace.
const { Cu } = require('chrome');
const { addDebuggerToGlobal } = Cu.import('resource://gre/modules/jsdebugger.jsm', {});
addDebuggerToGlobal(this);

exports['test resolve'] = function (assert) {
  let cuddlefish_id = 'sdk/loader/cuddlefish';
  assert.equal(resolve('../index.js', './dir/c.js'), './index.js');
  assert.equal(resolve('./index.js', './dir/c.js'), './dir/index.js');
  assert.equal(resolve('./dir/c.js', './index.js'), './dir/c.js');
  assert.equal(resolve('../utils/file.js', './dir/b.js'), './utils/file.js');

  assert.equal(resolve('../utils/./file.js', './dir/b.js'), './utils/file.js');
  assert.equal(resolve('../utils/file.js', './'), './../utils/file.js');
  assert.equal(resolve('./utils/file.js', './'), './utils/file.js');
  assert.equal(resolve('./utils/file.js', './index.js'), './utils/file.js');

  assert.equal(resolve('../utils/./file.js', cuddlefish_id), 'sdk/utils/file.js');
  assert.equal(resolve('../utils/file.js', cuddlefish_id), 'sdk/utils/file.js');
  assert.equal(resolve('./utils/file.js', cuddlefish_id), 'sdk/loader/utils/file.js');

  assert.equal(resolve('..//index.js', './dir/c.js'), './index.js');
  assert.equal(resolve('../../gre/modules/XPCOMUtils.jsm', 'resource://thing/utils/file.js'), 'resource://gre/modules/XPCOMUtils.jsm');
  assert.equal(resolve('../../gre/modules/XPCOMUtils.jsm', 'chrome://thing/utils/file.js'), 'chrome://gre/modules/XPCOMUtils.jsm');
  assert.equal(resolve('../../a/b/c.json', 'file:///thing/utils/file.js'), 'file:///a/b/c.json');

  // Does not change absolute paths
  assert.equal(resolve('resource://gre/modules/file.js', './dir/b.js'),
    'resource://gre/modules/file.js');
  assert.equal(resolve('file:///gre/modules/file.js', './dir/b.js'),
    'file:///gre/modules/file.js');
  assert.equal(resolve('/root.js', './dir/b.js'),
    '/root.js');
};

exports['test join'] = function (assert) {
  assert.equal(join('a/path', '../../../module'), '../module');
  assert.equal(join('a/path/to', '../module'), 'a/path/module');
  assert.equal(join('a/path/to', './module'), 'a/path/to/module');
  assert.equal(join('a/path/to', '././../module'), 'a/path/module');
  assert.equal(join('resource://my/path/yeah/yuh', '../whoa'),
    'resource://my/path/yeah/whoa');
  assert.equal(join('resource://my/path/yeah/yuh', './whoa'),
    'resource://my/path/yeah/yuh/whoa');
  assert.equal(join('file:///my/path/yeah/yuh', '../whoa'),
    'file:///my/path/yeah/whoa');
  assert.equal(join('file:///my/path/yeah/yuh', './whoa'),
    'file:///my/path/yeah/yuh/whoa');
  assert.equal(join('a/path/to', '..//module'), 'a/path/module');
};

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

exports['test invisibleToDebugger: false'] = function (assert) {
  let uri = root + '/fixtures/loader/cycles/';
  let loader = Loader({ paths: { '': uri } });
  main(loader, 'main');

  let dbg = new Debugger();
  let sandbox = loader.sandboxes[uri + 'main.js'];

  try {
    dbg.addDebuggee(sandbox);
    assert.ok(true, 'debugger added visible value');
  } catch(e) {
    assert.fail('debugger could not add visible value');
  }
};

exports['test invisibleToDebugger: true'] = function (assert) {
  let uri = root + '/fixtures/loader/cycles/';
  let loader = Loader({ paths: { '': uri }, invisibleToDebugger: true });
  main(loader, 'main');

  let dbg = new Debugger();
  let sandbox = loader.sandboxes[uri + 'main.js'];

  try {
    dbg.addDebuggee(sandbox);
    assert.fail('debugger added invisible value');
  } catch(e) {
    assert.ok(true, 'debugger did not add invisible value');
  }
};

require('test').run(exports);
