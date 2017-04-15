/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

var {
  Loader, main, unload, parseStack, resolve, nodeResolve
} = require('toolkit/loader');
var { readURI } = require('sdk/net/url');
var { all } = require('sdk/core/promise');
var { before, after } = require('sdk/test/utils');
var testOptions = require('@test/options');

var root = module.uri.substr(0, module.uri.lastIndexOf('/'))
// The following adds Debugger constructor to the global namespace.
const { Cc, Ci, Cu } = require('chrome');
const { addDebuggerToGlobal } = Cu.import('resource://gre/modules/jsdebugger.jsm', {});
addDebuggerToGlobal(this);

const { NetUtil } = Cu.import('resource://gre/modules/NetUtil.jsm', {});

const resProto = Cc["@mozilla.org/network/protocol;1?name=resource"]
        .getService(Ci.nsIResProtocolHandler);

const fileRoot = resProto.resolveURI(NetUtil.newURI(root));

let variants = [
  {
    description: "unpacked resource:",
    getRootURI(fixture) {
      return `${root}/fixtures/${fixture}/`;
    },
  },
  {
    description: "unpacked file:",
    getRootURI(fixture) {
      return `${fileRoot}/fixtures/${fixture}/`;
    },
  },
  {
    description: "packed resource:",
    getRootURI(fixture) {
      return `resource://${fixture}/`;
    },
  },
  {
    description: "packed jar:",
    getRootURI(fixture) {
      return `jar:${fileRoot}/fixtures/${fixture}.xpi!/`;
    },
  },
];

let fixtures = [
  'native-addon-test',
  'native-overrides-test',
];

for (let variant of variants) {
  exports[`test nodeResolve (${variant.description})`] = function (assert) {
    let rootURI = variant.getRootURI('native-addon-test');
    let manifest = {};
    manifest.dependencies = {};

    // Handles extensions
    resolveTest('../package.json', './dir/c.js', './package.json');
    resolveTest('../dir/b.js', './dir/c.js', './dir/b.js');

    resolveTest('./dir/b', './index.js', './dir/b.js');
    resolveTest('../index', './dir/b.js', './index.js');
    resolveTest('../', './dir/b.js', './index.js');
    resolveTest('./dir/a', './index.js', './dir/a.js', 'Precedence dir/a.js over dir/a/');
    resolveTest('../utils', './dir/a.js', './utils/index.js', 'Requiring a directory defaults to dir/index.js');
    resolveTest('../newmodule', './dir/c.js', './newmodule/lib/file.js', 'Uses package.json main in dir to load appropriate "main"');
    resolveTest('test-math', './utils/index.js', './node_modules/test-math/index.js',
      'Dependencies default to their index.js');
    resolveTest('test-custom-main', './utils/index.js', './node_modules/test-custom-main/lib/custom-entry.js',
      'Dependencies use "main" entry');
    resolveTest('test-math/lib/sqrt', './utils/index.js', './node_modules/test-math/lib/sqrt.js',
      'Dependencies\' files can be consumed via "/"');

    resolveTest('sdk/tabs/utils', './index.js', undefined,
      'correctly ignores SDK references in paths');
    resolveTest('fs', './index.js', undefined,
      'correctly ignores built in node modules in paths');

    resolveTest('test-add', './node_modules/test-math/index.js',
      './node_modules/test-math/node_modules/test-add/index.js',
      'Dependencies\' dependencies can be found');

    resolveTest('resource://gre/modules/commonjs/sdk/tabs.js', './index.js', undefined,
                'correctly ignores absolute URIs.');

    resolveTest('../tabs', 'resource://gre/modules/commonjs/sdk/addon/bootstrap.js', undefined,
                'correctly ignores attempts to resolve from a module at an absolute URI.');

    resolveTest('sdk/tabs', 'resource://gre/modules/commonjs/sdk/addon/bootstrap.js', undefined,
                'correctly ignores attempts to resolve from a module at an absolute URI.');

    function resolveTest (id, requirer, expected, msg) {
      let result = nodeResolve(id, requirer, { manifest: manifest, rootURI: rootURI });
      assert.equal(result, expected, 'nodeResolve ' + id + ' from ' + requirer + ' ' +msg);
    }
  }

  /*
  // TODO not working in current env
  exports[`test bundle (${variant.description`] = function (assert, done) {
    loadAddon('/native-addons/native-addon-test/')
  };
  */

  exports[`test native Loader overrides (${variant.description})`] = function*(assert) {
    const expectedKeys = Object.keys(require("sdk/io/fs")).join(", ");
    const manifest = yield getJSON('/fixtures/native-overrides-test/package.json');
    const rootURI = variant.getRootURI('native-overrides-test');

    let loader = Loader({
      paths: makePaths(rootURI),
      rootURI: rootURI,
      manifest: manifest,
      metadata: manifest,
      isNative: true
    });

    let program = main(loader);
    let fooKeys = Object.keys(program.foo).join(", ");
    let barKeys = Object.keys(program.foo).join(", ");
    let fsKeys = Object.keys(program.fs).join(", ");
    let overloadKeys = Object.keys(program.overload.fs).join(", ");
    let overloadLibKeys = Object.keys(program.overloadLib.fs).join(", ");

    assert.equal(fooKeys, expectedKeys, "foo exports sdk/io/fs");
    assert.equal(barKeys, expectedKeys, "bar exports sdk/io/fs");
    assert.equal(fsKeys, expectedKeys, "sdk/io/fs exports sdk/io/fs");
    assert.equal(overloadKeys, expectedKeys, "overload exports foo which exports sdk/io/fs");
    assert.equal(overloadLibKeys, expectedKeys, "overload/lib/foo exports foo/lib/foo");
    assert.equal(program.internal, "test", "internal exports ./lib/internal");
    assert.equal(program.extra, true, "fs-extra was exported properly");

    assert.equal(program.Tabs, "no tabs exist", "sdk/tabs exports ./lib/tabs from the add-on");
    assert.equal(program.CoolTabs, "no tabs exist", "sdk/tabs exports ./lib/tabs from the node_modules");
    assert.equal(program.CoolTabsLib, "a cool tabs implementation", "./lib/tabs true relative path from the node_modules");

    assert.equal(program.ignore, "do not ignore this export", "../ignore override was ignored.");

    unload(loader);
  };

  exports[`test invalid native Loader overrides cause no errors (${variant.description})`] = function*(assert) {
    const manifest = yield getJSON('/fixtures/native-overrides-test/package.json');
    const rootURI = variant.getRootURI('native-overrides-test');
    const EXPECTED = JSON.stringify({});

    let makeLoader = (rootURI, manifest) => Loader({
      paths: makePaths(rootURI),
      rootURI: rootURI,
      manifest: manifest,
      metadata: manifest,
      isNative: true
    });

    manifest.jetpack.overrides = "string";
    let loader = makeLoader(rootURI, manifest);
    assert.equal(JSON.stringify(loader.manifest.jetpack.overrides), EXPECTED,
                 "setting jetpack.overrides to a string caused no errors making the loader");
    unload(loader);

    manifest.jetpack.overrides = true;
    loader = makeLoader(rootURI, manifest);
    assert.equal(JSON.stringify(loader.manifest.jetpack.overrides), EXPECTED,
                 "setting jetpack.overrides to a boolean caused no errors making the loader");
    unload(loader);

    manifest.jetpack.overrides = 5;
    loader = makeLoader(rootURI, manifest);
    assert.equal(JSON.stringify(loader.manifest.jetpack.overrides), EXPECTED,
                 "setting jetpack.overrides to a number caused no errors making the loader");
    unload(loader);

    manifest.jetpack.overrides = null;
    loader = makeLoader(rootURI, manifest);
    assert.equal(JSON.stringify(loader.manifest.jetpack.overrides), EXPECTED,
                 "setting jetpack.overrides to null caused no errors making the loader");
    unload(loader);
  };

  exports[`test invalid native Loader jetpack key cause no errors (${variant.description})`] = function*(assert) {
    const manifest = yield getJSON('/fixtures/native-overrides-test/package.json');
    const rootURI = variant.getRootURI('native-overrides-test');
    const EXPECTED = JSON.stringify({});

    let makeLoader = (rootURI, manifest) => Loader({
      paths: makePaths(rootURI),
      rootURI: rootURI,
      manifest: manifest,
      metadata: manifest,
      isNative: true
    });

    manifest.jetpack = "string";
    let loader = makeLoader(rootURI, manifest);
    assert.equal(JSON.stringify(loader.manifest.jetpack.overrides), EXPECTED,
                 "setting jetpack.overrides to a string caused no errors making the loader");
    unload(loader);

    manifest.jetpack = true;
    loader = makeLoader(rootURI, manifest);
    assert.equal(JSON.stringify(loader.manifest.jetpack.overrides), EXPECTED,
                 "setting jetpack.overrides to a boolean caused no errors making the loader");
    unload(loader);

    manifest.jetpack = 5;
    loader = makeLoader(rootURI, manifest);
    assert.equal(JSON.stringify(loader.manifest.jetpack.overrides), EXPECTED,
                 "setting jetpack.overrides to a number caused no errors making the loader");
    unload(loader);

    manifest.jetpack = null;
    loader = makeLoader(rootURI, manifest);
    assert.equal(JSON.stringify(loader.manifest.jetpack.overrides), EXPECTED,
                 "setting jetpack.overrides to null caused no errors making the loader");
    unload(loader);
  };

  exports[`test native Loader without mappings (${variant.description})`] = function (assert, done) {
    getJSON('/fixtures/native-addon-test/package.json').then(manifest => {
      let rootURI = variant.getRootURI('native-addon-test');
      let loader = Loader({
        paths: makePaths(rootURI),
        rootURI: rootURI,
        manifest: manifest,
        isNative: true
      });

      let program = main(loader);
      testLoader(program, assert);
      unload(loader);
      done();
    }).then(null, (reason) => console.error(reason));
  };

  exports[`test require#resolve with relative, dependencies (${variant.description})`] = function(assert, done) {
    getJSON('/fixtures/native-addon-test/package.json').then(manifest => {
      let rootURI = variant.getRootURI('native-addon-test');
      let loader = Loader({
        paths: makePaths(rootURI),
        rootURI: rootURI,
        manifest: manifest,
        isNative: true
      });

      let program = main(loader);
      let fixtureRoot = program.require.resolve("./").replace(/index\.js$/, "");

      assert.equal(variant.getRootURI("native-addon-test"), fixtureRoot, "correct resolution root");
      assert.equal(program.require.resolve("test-math"), fixtureRoot + "node_modules/test-math/index.js", "works with node_modules");
      assert.equal(program.require.resolve("./newmodule"), fixtureRoot + "newmodule/lib/file.js", "works with directory mains");
      assert.equal(program.require.resolve("./dir/a"), fixtureRoot + "dir/a.js", "works with normal relative module lookups");
      assert.equal(program.require.resolve("modules/Promise.jsm"), "resource://gre/modules/Promise.jsm", "works with path lookups");

      // TODO bug 1050422, handle loading non JS/JSM file paths
      // assert.equal(program.require.resolve("test-assets/styles.css"), fixtureRoot + "node_modules/test-assets/styles.css",
      // "works with different file extension lookups in dependencies");

      unload(loader);
      done();
    }).then(null, (reason) => console.error(reason));
  };
}

before(exports, () => {
  for (let fixture of fixtures) {
    let url = `jar:${root}/fixtures/${fixture}.xpi!/`;

    resProto.setSubstitution(fixture, NetUtil.newURI(url));
  }
});

after(exports, () => {
  for (let fixture of fixtures)
    resProto.setSubstitution(fixture, null);
});

exports['test JSM loading'] = function (assert, done) {
  getJSON('/fixtures/jsm-package/package.json').then(manifest => {
    let rootURI = root + '/fixtures/jsm-package/';
    let loader = Loader({
      paths: makePaths(rootURI),
      rootURI: rootURI,
      manifest: manifest,
      isNative: true
    });

    let program = main(loader);
    assert.ok(program.localJSMCached, 'local relative JSMs are cached');
    assert.ok(program.isCachedJSAbsolute , 'absolute resource:// js are cached');
    assert.ok(program.isCachedPath, 'JSMs resolved in paths are cached');
    assert.ok(program.isCachedAbsolute, 'absolute resource:// JSMs are cached');

    assert.ok(program.localJSM, 'able to load local relative JSMs');
    all([
      program.isLoadedPath(10),
      program.isLoadedAbsolute(20),
      program.isLoadedJSAbsolute(30)
    ]).then(([path, absolute, jsabsolute]) => {
      assert.equal(path, 10, 'JSM files resolved from path work');
      assert.equal(absolute, 20, 'JSM files resolved from full resource:// work');
      assert.equal(jsabsolute, 30, 'JS files resolved from full resource:// work');
    }).then(done, console.error);

  }).then(null, console.error);
};

function testLoader (program, assert) {
  // Test 'main' entries
  // no relative custom main `lib/index.js`
  assert.equal(program.customMainModule, 'custom entry file',
    'a node_module dependency correctly uses its `main` entry in manifest');
  // relative custom main `./lib/index.js`
  assert.equal(program.customMainModuleRelative, 'custom entry file relative',
    'a node_module dependency correctly uses its `main` entry in manifest with relative ./');
  // implicit './index.js'
  assert.equal(program.defaultMain, 'default main',
    'a node_module dependency correctly defautls to index.js for main');

  // Test directory exports
  assert.equal(program.directoryDefaults, 'utils',
    '`require`ing a directory defaults to dir/index.js');
  assert.equal(program.directoryMain, 'main from new module',
    '`require`ing a directory correctly loads the `main` entry and not index.js');
  assert.equal(program.resolvesJSoverDir, 'dir/a',
    '`require`ing "a" resolves "a.js" over "a/index.js"');

  // Test dependency's dependencies
  assert.ok(program.math.add,
    'correctly defaults to index.js of a module');
  assert.equal(program.math.add(10, 5), 15,
    'node dependencies correctly include their own dependencies');
  assert.equal(program.math.subtract(10, 5), 5,
    'node dependencies correctly include their own dependencies');
  assert.equal(program.mathInRelative.subtract(10, 5), 5,
    'relative modules can also include node dependencies');

  // Test SDK natives
  assert.ok(program.promise.defer, 'main entry can include SDK modules with no deps');
  assert.ok(program.promise.resolve, 'main entry can include SDK modules with no deps');
  assert.ok(program.eventCore.on, 'main entry can include SDK modules that have dependencies');
  assert.ok(program.eventCore.off, 'main entry can include SDK modules that have dependencies');

  // Test JSMs
  assert.ok(program.promisejsm.defer, 'can require JSM files in path');
  assert.equal(program.localJSM.test, 'this is a jsm',
    'can require relative JSM files');

  // Other tests
  assert.equal(program.areModulesCached, true,
    'modules are correctly cached');
  assert.equal(program.testJSON.dependencies['test-math'], '*',
    'correctly requires JSON files');
}

function getJSON (uri) {
  return readURI(root + uri).then(manifest => JSON.parse(manifest));
}

function makePaths (uri) {
  // Uses development SDK modules if overloaded in loader
  let sdkPaths = testOptions.paths ? testOptions.paths[''] : 'resource://gre/modules/commonjs/';
  return {
    './': uri,
    'sdk/': sdkPaths + 'sdk/',
    'toolkit/': sdkPaths + 'toolkit/',
    'modules/': 'resource://gre/modules/'
  };
}

function loadAddon (uri, map) {
  let rootURI = root + uri;
  getJSON(uri + '/package.json').then(manifest => {
    let loader = Loader({
      paths: makePaths(rootURI),
      rootURI: rootURI,
      manifest: manifest,
      isNative: true,
      modules: {
        '@test/options': testOptions
      }
    });
    let program = main(loader);
  }).then(null, console.error);
}

require('sdk/test').run(exports);
