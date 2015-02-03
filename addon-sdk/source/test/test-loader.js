/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

let {
  Loader, main, unload, parseStack, generateMap, resolve, join,
  Require, Module
} = require('toolkit/loader');
let { readURI } = require('sdk/net/url');

let root = module.uri.substr(0, module.uri.lastIndexOf('/'));

const app = require('sdk/system/xul-app');

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

exports['test sandboxes are not added if error'] = function (assert) {
  let uri = root + '/fixtures/loader/missing-twice/';
  let loader = Loader({ paths: { '': uri } });
  let program = main(loader, 'main');
  assert.ok(!(uri + 'not-found.js' in loader.sandboxes), 'not-found.js not in loader.sandboxes');
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

exports["test invalid module not cached and throws everytime"] = function(assert) {
  let uri = root + "/fixtures/loader/missing-twice/";
  let loader = Loader({ paths: { "": uri } });

  let { firstError, secondError, invalidJSON1, invalidJSON2 } = main(loader, "main");
  assert.equal(firstError.message, "Module `not-found` is not found at " +
    uri + "not-found.js", "throws on first invalid require");
  assert.equal(firstError.lineNumber, 8, "first error is on line 7");
  assert.equal(secondError.message, "Module `not-found` is not found at " +
    uri + "not-found.js", "throws on second invalid require");
  assert.equal(secondError.lineNumber, 14, "second error is on line 14");

  assert.equal(invalidJSON1.message,
    "JSON.parse: unexpected character at line 1 column 1 of the JSON data",
    "throws on invalid JSON");
  assert.equal(invalidJSON2.message,
    "JSON.parse: unexpected character at line 1 column 1 of the JSON data",
    "throws on invalid JSON second time");
};

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

exports['test console global by default'] = function (assert) {
  let uri = root + '/fixtures/loader/globals/';
  let loader = Loader({ paths: { '': uri }});
  let program = main(loader, 'main');

  assert.ok(typeof program.console === 'object', 'global `console` exists');
  assert.ok(typeof program.console.log === 'function', 'global `console.log` exists');

  let loader2 = Loader({ paths: { '': uri }, globals: { console: fakeConsole }});
  let program2 = main(loader2, 'main');

  assert.equal(program2.console, fakeConsole,
    'global console can be overridden with Loader options');
  function fakeConsole () {};
};

exports['test shared globals'] = function(assert) {
  let uri = root + '/fixtures/loader/cycles/';
  let loader = Loader({ paths: { '': uri }, sharedGlobal: true,
                        sharedGlobalBlacklist: ['b'] });

  let program = main(loader, 'main');

  // As it is hard to verify what is the global of an object
  // (due to wrappers) we check that we see the `foo` symbol
  // being manually injected into the shared global object
  loader.sharedGlobalSandbox.foo = true;

  let m = loader.sandboxes[uri + 'main.js'];
  let a = loader.sandboxes[uri + 'a.js'];
  let b = loader.sandboxes[uri + 'b.js'];

  assert.ok(Cu.getGlobalForObject(m).foo, "main is shared");
  assert.ok(Cu.getGlobalForObject(a).foo, "a is shared");
  assert.ok(!Cu.getGlobalForObject(b).foo, "b isn't shared");

  unload(loader);
}

exports["test require#resolve"] = function(assert) {
  let foundRoot = require.resolve("sdk/tabs").replace(/sdk\/tabs.js$/, "");
  assert.ok(root, foundRoot, "correct resolution root");

  assert.equal(foundRoot + "sdk/tabs.js", require.resolve("sdk/tabs"), "correct resolution of sdk module");
  assert.equal(foundRoot + "toolkit/loader.js", require.resolve("toolkit/loader"), "correct resolution of sdk module");
};

const modulesURI = require.resolve("toolkit/loader").replace("toolkit/loader.js", "");
exports["test loading a loader"] = function(assert) {
  const loader = Loader({ paths: { "": modulesURI } });

  const require = Require(loader, module);

  const requiredLoader = require("toolkit/loader");

  assert.equal(requiredLoader.Loader, Loader,
               "got the same Loader instance");

  const jsmLoader = Cu.import(require.resolve("toolkit/loader"), {}).Loader;

  assert.equal(jsmLoader.Loader, requiredLoader.Loader,
               "loading loader via jsm returns same loader");

  unload(loader);
};

exports['test loader on unsupported modules with checkCompatibility true'] = function(assert) {
  let loader = Loader({
    paths: { '': root + "/" },
    checkCompatibility: true
  });
  let require = Require(loader, module);

  assert.throws(() => {
    if (!app.is('Firefox')) {
      require('fixtures/loader/unsupported/firefox');
    }
    else {
      require('fixtures/loader/unsupported/fennec');
    }
  }, /^Unsupported Application/, "throws Unsupported Application");

  unload(loader);
};

exports['test loader on unsupported modules with checkCompatibility false'] = function(assert) {
  let loader = Loader({
    paths: { '': root + "/" },
    checkCompatibility: false
  });
  let require = Require(loader, module);

  try {
    if (!app.is('Firefox')) {
      require('fixtures/loader/unsupported/firefox');
    }
    else {
      require('fixtures/loader/unsupported/fennec');
    }
    assert.pass("loaded unsupported module without an error");
  }
  catch(e) {
    assert.fail(e);
  }

  unload(loader);
};

exports['test loader on unsupported modules with checkCompatibility default'] = function(assert) {
  let loader = Loader({ paths: { '': root + "/" } });
  let require = Require(loader, module);

  try {
    if (!app.is('Firefox')) {
      require('fixtures/loader/unsupported/firefox');
    }
    else {
      require('fixtures/loader/unsupported/fennec');
    }
    assert.pass("loaded unsupported module without an error");
  }
  catch(e) {
    assert.fail(e);
  }

  unload(loader);
};

exports["test Cu.import of toolkit/loader"] = (assert) => {
  const toolkitLoaderURI = require.resolve("toolkit/loader");
  const loaderModule = Cu.import(toolkitLoaderURI).Loader;
  const { Loader, Require, Main } = loaderModule;
  const version = "0.1.0";
  const id = `fxos_${version.replace(".", "_")}_simulator@mozilla.org`;
  const uri = `resource://${encodeURIComponent(id.replace("@", "at"))}/`;

  const loader = Loader({
    paths: {
      "./": uri + "lib/",
      // Can't just put `resource://gre/modules/commonjs/` as it
      // won't take module overriding into account.
      "": toolkitLoaderURI.replace("toolkit/loader.js", "")
    },
    globals: {
      console: console
    },
    modules: {
      "toolkit/loader": loaderModule,
      addon: {
        id: "simulator",
        version: "0.1",
        uri: uri
      }
    }
  });

  let require_ = Require(loader, { id: "./addon" });
  assert.equal(typeof(loaderModule),
               typeof(require_("toolkit/loader")),
               "module returned is whatever was mapped to it");
};

exports["test Cu.import in b2g style"] = (assert) => {
  const {FakeCu} = require("./loader/b2g");
  const toolkitLoaderURI = require.resolve("toolkit/loader");
  const b2g = new FakeCu();

  const exported = {};
  const loader = b2g.import(toolkitLoaderURI, exported);

  assert.equal(typeof(exported.Loader),
               "function",
               "loader is a function");
  assert.equal(typeof(exported.Loader.Loader),
               "function",
               "Loader.Loader is a funciton");
};

require('sdk/test').run(exports);
