/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that source URLs in the Web Console can be clicked to display the
// standard View Source window.

function test() {
  define('gclitest/requirable', [], function(require, exports, module) {
    exports.thing1 = 'thing1';
    exports.thing2 = 2;

    let status = 'initial';
    exports.setStatus = function(aStatus) { status = aStatus; };
    exports.getStatus = function() { return status; };
  });

  define('gclitest/unrequirable', [], function(require, exports, module) {
    null.throwNPE();
  });

  define('gclitest/recurse', [], function(require, exports, module) {
    require('gclitest/recurse');
  });

  testWorking();
  testLeakage();
  testMultiImport();
  testRecursive();
  testUncompilable();

  delete define.modules['gclitest/requirable'];
  delete define.globalDomain.modules['gclitest/requirable'];
  delete define.modules['gclitest/unrequirable'];
  delete define.globalDomain.modules['gclitest/unrequirable'];
  delete define.modules['gclitest/recurse'];
  delete define.globalDomain.modules['gclitest/recurse'];

  finish();
}

function testWorking() {
  // There are lots of requirement tests that we could be doing here
  // The fact that we can get anything at all working is a testament to
  // require doing what it should - we don't need to test the
  let requireable = require('gclitest/requirable');
  ok('thing1' == requireable.thing1, 'thing1 was required');
  ok(2 == requireable.thing2, 'thing2 was required');
  ok(requireable.thing3 === undefined, 'thing3 was not required');
}

function testDomains() {
  let requireable = require('gclitest/requirable');
  ok(requireable.status === undefined, 'requirable has no status');
  requireable.setStatus(null);
  ok(null === requireable.getStatus(), 'requirable.getStatus changed to null');
  ok(requireable.status === undefined, 'requirable still has no status');
  requireable.setStatus('42');
  ok('42' == requireable.getStatus(), 'requirable.getStatus changed to 42');
  ok(requireable.status === undefined, 'requirable *still* has no status');

  let domain = new define.Domain();
  let requireable2 = domain.require('gclitest/requirable');
  ok(requireable2.status === undefined, 'requirable2 has no status');
  ok('initial' === requireable2.getStatus(), 'requirable2.getStatus is initial');
  requireable2.setStatus(999);
  ok(999 === requireable2.getStatus(), 'requirable2.getStatus changed to 999');
  ok(requireable2.status === undefined, 'requirable2 still has no status');

  t.verifyEqual('42', requireable.getStatus());
  ok(requireable.status === undefined, 'requirable has no status (as expected)');

  delete domain.modules['gclitest/requirable'];
}

function testLeakage() {
  let requireable = require('gclitest/requirable');
  ok(requireable.setup == null, 'leakage of setup');
  ok(requireable.shutdown == null, 'leakage of shutdown');
  ok(requireable.testWorking == null, 'leakage of testWorking');
}

function testMultiImport() {
  let r1 = require('gclitest/requirable');
  let r2 = require('gclitest/requirable');
  ok(r1 === r2, 'double require was strict equal');
}

function testUncompilable() {
  // It's not totally clear how a module loader should perform with unusable
  // modules, however at least it should go into a flat spin ...
  // GCLI mini_require reports an error as it should
  try {
    let unrequireable = require('gclitest/unrequirable');
    fail();
  }
  catch (ex) {
    // an exception is expected
  }
}

function testRecursive() {
  // See Bug 658583
  // require('gclitest/recurse');
  // Also see the comments in the testRecursive() function
}
