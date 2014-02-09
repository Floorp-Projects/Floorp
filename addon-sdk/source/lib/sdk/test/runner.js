/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental"
};

var { setTimeout } = require("../timers");
var { exit, stdout } = require("../system");
var cfxArgs = require("@test/options");

function runTests(findAndRunTests) {
  var harness = require("./harness");

  function onDone(tests) {
    stdout.write("\n");
    var total = tests.passed + tests.failed;
    stdout.write(tests.passed + " of " + total + " tests passed.\n");

    if (tests.failed == 0) {
      if (tests.passed === 0) {
        stdout.write("No tests were run\n");
      }

      setTimeout(function() {
        if (cfxArgs.parseable) {
          console.log('calling exit(0)');
        }
        exit(0);
      }, 0);
    } else {
      if (cfxArgs.verbose || cfxArgs.parseable)
        printFailedTests(tests, stdout.write);
      exit(1);
    }
  };

  // We may have to run test on next cycle, otherwise XPCOM components
  // are not correctly updated.
  // For ex: nsIFocusManager.getFocusedElementForWindow may throw
  // NS_ERROR_ILLEGAL_VALUE exception.
  require("../timers").setTimeout(function () {
    harness.runTests({
      findAndRunTests: findAndRunTests,
      iterations: cfxArgs.iterations || 1,
      filter: cfxArgs.filter,
      profileMemory: cfxArgs.profileMemory,
      stopOnError: cfxArgs.stopOnError,
      verbose: cfxArgs.verbose,
      parseable: cfxArgs.parseable,
      print: stdout.write,
      onDone: onDone
    });
  }, 0);
}

function printFailedTests(tests, print) {
  let iterationNumber = 0;
  let singleIteration = tests.testRuns.length == 1;
  let padding = singleIteration ? "" : "  ";

  print("\nThe following tests failed:\n");

  for each (let testRun in tests.testRuns) {
    iterationNumber++;

    if (!singleIteration)
      print("  Iteration " + iterationNumber + ":\n");

    for each (let test in testRun) {
      if (test.failed > 0) {
        print(padding + "  " + test.name + ": " + test.errors +"\n");
      }
    }
    print("\n");
  }
}

function main() {
  var testsStarted = false;

  if (!testsStarted) {
    testsStarted = true;
    runTests(function findAndRunTests(loader, nextIteration) {
      loader.require("../deprecated/unit-test").findAndRunTests({
        testOutOfProcess: false,
        testInProcess: true,
        stopOnError: cfxArgs.stopOnError,
        filter: cfxArgs.filter,
        onDone: nextIteration
      });
    });
  }
};

if (require.main === module)
  main();

exports.runTestsFromModule = function runTestsFromModule(module) {
  let id = module.id;
  // Make a copy of exports as it may already be frozen by module loader
  let exports = {};
  Object.keys(module.exports).forEach(function(key) {
    exports[key] = module.exports[key];
  });

  runTests(function findAndRunTests(loader, nextIteration) {
    // Consider that all these tests are CommonJS ones
    loader.require('../../test').run(exports);

    // Reproduce what is done in unit-test-finder.findTests()
    let tests = [];
    for each (let name in Object.keys(exports).sort()) {
      tests.push({
        setup: exports.setup,
        teardown: exports.teardown,
        testFunction: exports[name],
        name: id + "." + name
      });
    }

    // Reproduce what is done by unit-test.findAndRunTests()
    var { TestRunner } = loader.require("../deprecated/unit-test");
    var runner = new TestRunner();
    runner.startMany({
      tests: tests,
      stopOnError: cfxArgs.stopOnError,
      onDone: nextIteration
    });
  });
}
