/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

exports.testDefine = function(assert) {
  let tiger = require('./modules/tiger');
  assert.equal(tiger.name, 'tiger', 'name proprety was exported properly');
  assert.equal(tiger.type, 'cat', 'property form other module exported');
};

exports.testDefineInoresNonFactory = function(assert) {
  let mod = require('./modules/async2');
  assert.equal(mod.name, 'async2', 'name proprety was exported properly');
  assert.ok(mod.traditional2Name !== 'traditional2', '1st is ignored');
};
/* Disable test that require AMD specific functionality:

// define() that exports a function as the module value,
// specifying a module name.
exports.testDefExport = function(assert) {
  var add = require('modules/add');
  assert.equal(add(1, 1), 2, 'Named define() exporting a function');
};

// define() that exports function as a value, but is anonymous
exports.testAnonDefExport = function (assert) {
  var subtract = require('modules/subtract');
  assert.equal(subtract(4, 2), 2,
                   'Anonymous define() exporting a function');
}

// using require([], function () {}) to load modules.
exports.testSimpleRequire = function (assert) {
  require(['modules/blue', 'modules/orange'], function (blue, orange) {
    assert.equal(blue.name, 'blue', 'Simple require for blue');
    assert.equal(orange.name, 'orange', 'Simple require for orange');
    assert.equal(orange.parentType, 'color',
                     'Simple require dependency check for orange');
  });
}

// using nested require([]) calls.
exports.testSimpleRequireNested = function (assert) {
  require(['modules/blue', 'modules/orange', 'modules/green'],
  function (blue, orange, green) {

    require(['modules/orange', 'modules/red'], function (orange, red) {
      assert.equal(red.name, 'red', 'Simple require for red');
      assert.equal(red.parentType, 'color',
                       'Simple require dependency check for red');
      assert.equal(blue.name, 'blue', 'Simple require for blue');
      assert.equal(orange.name, 'orange', 'Simple require for orange');
      assert.equal(orange.parentType, 'color',
                       'Simple require dependency check for orange');
      assert.equal(green.name, 'green', 'Simple require for green');
      assert.equal(green.parentType, 'color',
                       'Simple require dependency check for green');
    });

  });
}

// requiring a traditional module, that uses async, that use traditional and
// async, with a circular reference
exports.testMixedCircular = function (assert) {
  var t = require('modules/traditional1');
  assert.equal(t.name, 'traditional1', 'Testing name');
  assert.equal(t.traditional2Name, 'traditional2',
                   'Testing dependent name');
  assert.equal(t.traditional1Name, 'traditional1', 'Testing circular name');
  assert.equal(t.async2Name, 'async2', 'Testing async2 name');
  assert.equal(t.async2Traditional2Name, 'traditional2',
                   'Testing nested traditional2 name');
}

// Testing define()(function(require) {}) with some that use exports,
// some that use return.
exports.testAnonExportsReturn = function (assert) {
  var lion = require('modules/lion');
  require(['modules/tiger', 'modules/cheetah'], function (tiger, cheetah) {
    assert.equal('lion', lion, 'Check lion name');
    assert.equal('tiger', tiger.name, 'Check tiger name');
    assert.equal('cat', tiger.type, 'Check tiger type');
    assert.equal('cheetah', cheetah(), 'Check cheetah name');
  });
}

// circular dependency
exports.testCircular = function (assert) {
  var pollux = require('modules/pollux'),
      castor = require('modules/castor');

  assert.equal(pollux.name, 'pollux', 'Pollux\'s name');
  assert.equal(pollux.getCastorName(),
                   'castor', 'Castor\'s name from Pollux.');
  assert.equal(castor.name, 'castor', 'Castor\'s name');
  assert.equal(castor.getPolluxName(), 'pollux',
                   'Pollux\'s name from Castor.');
}

// test a bad module that asks for exports but also does a define() return
exports.testBadExportAndReturn = function (assert) {
  var passed = false;
  try {
    var bad = require('modules/badExportAndReturn');
  } catch(e) {
    passed = /cannot use exports and also return/.test(e.toString());
  }
  assert.equal(passed, true, 'Make sure exports and return fail');
}

// test a bad circular dependency, where an exported value is needed, but
// the return value happens too late, a module already asked for the exported
// value.
exports.testBadExportAndReturnCircular = function (assert) {
  var passed = false;
  try {
    var bad = require('modules/badFirst');
  } catch(e) {
    passed = /after another module has referenced its exported value/
             .test(e.toString());
  }
  assert.equal(passed, true, 'Make sure return after an exported ' +
                                 'value is grabbed by another module fails.');
}

// only allow one define call per file.
exports.testOneDefine = function (assert) {
  var passed = false;
  try {
    var dupe = require('modules/dupe');
  } catch(e) {
    passed = /Only one call to define/.test(e.toString());
  }
  assert.equal(passed, true, 'Only allow one define call per module');
}

// only allow one define call per file, testing a bad nested define call.
exports.testOneDefineNested = function (assert) {
  var passed = false;
  try {
    var dupe = require('modules/dupeNested');
  } catch(e) {
    passed = /Only one call to define/.test(e.toString());
  }
  assert.equal(passed, true, 'Only allow one define call per module');
}
*/

require('sdk/test').run(exports);
