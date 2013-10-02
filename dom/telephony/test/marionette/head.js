/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

let Promise = SpecialPowers.Cu.import("resource://gre/modules/Promise.jsm").Promise;

/**
 * Emulator helper.
 */
let emulator = (function() {
  let pendingCmdCount = 0;
  let originalRunEmulatorCmd = runEmulatorCmd;

  // Overwritten it so people could not call this function directly.
  runEmulatorCmd = function() {
    throw "Use emulator.run(cmd, callback) instead of runEmulatorCmd";
  };

  function run(cmd, callback) {
    pendingCmdCount++;
    originalRunEmulatorCmd(cmd, function(result) {
      pendingCmdCount--;
      if (callback && typeof callback === "function") {
        callback(result);
      }
    });
  }

  /**
   * @return Promise
   */
  function waitFinish() {
    let deferred = Promise.defer();

    waitFor(function() {
      deferred.resolve();
    }, function() {
      return pendingCmdCount === 0;
    });

    return deferred.promise;
  }

  return {
    run: run,
    waitFinish: waitFinish
  };
}());

function startTest(test) {
  // Extend finish() with tear down.
  finish = (function() {
    let originalFinish = finish;

    function tearDown() {
      log('== Test TearDown ==');
      emulator.waitFinish()
        .then(function() {
          originalFinish.apply(this, arguments);
        });
    }

    return tearDown.bind(this);
  }());

  // Start the test.
  test();
}
