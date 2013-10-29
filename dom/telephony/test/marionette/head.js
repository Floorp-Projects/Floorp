/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let Promise = SpecialPowers.Cu.import("resource://gre/modules/Promise.jsm").Promise;
let telephony;

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

/**
 * Telephony related helper functions.
 */
(function() {
  function checkInitialState() {
    log("Verify initial state.");
    ok(telephony, 'telephony');
    is(telephony.active, null, 'telephony.active');
    ok(telephony.calls, 'telephony.calls');
    is(telephony.calls.length, 0, 'telephony.calls.length');
  }

  /**
   * @return Promise
   */
  function clearCalls() {
    let deferred = Promise.defer();

    log("Clear existing calls.");
    emulator.run("gsm clear", function(result) {
      if (result[0] == "OK") {
        waitFor(function() {
          deferred.resolve();
        }, function() {
          return telephony.calls.length === 0;
        });
      } else {
        log("Failed to clear existing calls.");
        deferred.reject();
      }
    });

    return deferred.promise;
  }

  this.checkInitialState = checkInitialState;
  this.clearCalls = clearCalls;
}());

function _startTest(permissions, test) {
  function permissionSetUp() {
    SpecialPowers.setBoolPref("dom.mozSettings.enabled", true);
    SpecialPowers.setBoolPref("dom.promise.enabled", true);
    for (let per of permissions) {
      SpecialPowers.addPermission(per, true, document);
    }
  }

  function permissionTearDown() {
    SpecialPowers.clearUserPref("dom.mozSettings.enabled");
    SpecialPowers.clearUserPref("dom.promise.enabled");
    for (let per of permissions) {
      SpecialPowers.removePermission(per, document);
    }
  }

  function setUp() {
    log('== Test SetUp ==');
    permissionSetUp();
    // Make sure that we get the telephony after adding permission.
    telephony = window.navigator.mozTelephony;
    ok(telephony);
    return clearCalls().then(checkInitialState);
  }

  // Extend finish() with tear down.
  finish = (function() {
    let originalFinish = finish;

    function tearDown() {
      log('== Test TearDown ==');
      emulator.waitFinish()
        .then(permissionTearDown)
        .then(function() {
          originalFinish.apply(this, arguments);
        });
    }

    return tearDown.bind(this);
  }());

  function mainTest() {
    setUp()
      .then(function onSuccess() {
        log('== Test Start ==');
        test();
      }, function onError(error) {
        SpecialPowers.Cu.reportError(error);
        ok(false, 'SetUp error');
      });
  }

  mainTest();
}

function startTest(test) {
  _startTest(['telephony'], test);
}

function startTestWithPermissions(permissions, test) {
  _startTest(permissions.concat('telephony'), test);
}
