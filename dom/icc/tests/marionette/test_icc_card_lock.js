/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

let icc = navigator.mozIccManager;
ok(icc instanceof MozIccManager, "icc is instanceof " + icc.constructor);

/* Reset pin retries by passing correct pin code. */
function resetPinRetries(pin, callback) {
  let request = icc.setCardLock(
    {lockType: "pin",
     pin: pin,
     newPin: pin});

  request.onsuccess = function onsuccess() {
    callback();
  };

  request.onerror = function onerror() {
    is(false, "Reset pin retries got error: " + request.error.name);
    callback();
  };
}

/* Test PIN code changes fail */
function testPinChangeFailed() {
  // The default pin is '0000' in emulator
  let request = icc.setCardLock(
    {lockType: "pin",
     pin: "1111",
     newPin: "0000"});

  ok(request instanceof DOMRequest,
     "request instanceof " + request.constructor);

  request.onerror = function onerror() {
    is(request.error.name, "IncorrectPassword");
    is(request.error.lockType, "pin");
    // The default pin retries is 3, failed once becomes to 2
    is(request.error.retryCount, 2);

    resetPinRetries("0000", runNextTest);
  };
}

/* Test PIN code changes success */
function testPinChangeSuccess() {
  // The default pin is '0000' in emulator
  let request = icc.setCardLock(
    {lockType: "pin",
     pin: "0000",
     newPin: "0000"});

  ok(request instanceof DOMRequest,
     "request instanceof " + request.constructor);

  request.onerror = function onerror() {
    ok(false, "Should not fail, got error: " + request.error.name);

    runNextTest();
  };

  request.onsuccess = function onsuccess() {
    is(request.result.lockType, "pin");
    is(request.result.success, true);

    runNextTest();
  };
}

/* Read PIN-lock retry count */
function testPinCardLockRetryCount() {
  let request = icc.getCardLockRetryCount('pin');

  ok(request instanceof DOMRequest,
     'request instanceof ' + request.constructor);

  request.onsuccess = function onsuccess() {
    is(request.result.lockType, 'pin',
        'lockType is ' + request.result.lockType);
    ok(request.result.retryCount >= 0,
        'retryCount is ' + request.result.retryCount);
    runNextTest();
  };
  request.onerror = function onerror() {
    // The operation is optional any might not be supported for all
    // all locks. In this case, we generate 'NotSupportedError' for
    // the valid lock types.
    is(request.error.name, 'RequestNotSupported',
        'error name is ' + request.error.name);
    runNextTest();
  };
}

/* Read PUK-lock retry count */
function testPukCardLockRetryCount() {
  let request = icc.getCardLockRetryCount('puk');

  ok(request instanceof DOMRequest,
     'request instanceof ' + request.constructor);

  request.onsuccess = function onsuccess() {
    is(request.result.lockType, 'puk',
        'lockType is ' + request.result.lockType);
    ok(request.result.retryCount >= 0,
        'retryCount is ' + request.result.retryCount);
    runNextTest();
  };
  request.onerror = function onerror() {
    // The operation is optional any might not be supported for all
    // all locks. In this case, we generate 'NotSupportedError' for
    // the valid lock types.
    is(request.error.name, 'RequestNotSupported',
        'error name is ' + request.error.name);
    runNextTest();
  };
}

/* Read lock retry count for an invalid entries  */
function testInvalidCardLockRetryCount() {
  let request = icc.getCardLockRetryCount('invalid-lock-type');

  ok(request instanceof DOMRequest,
     'request instanceof ' + request.constructor);

  request.onsuccess = function onsuccess() {
    ok(false,
        'request should never return success for an invalid lock type');
    runNextTest();
  };
  request.onerror = function onerror() {
    is(request.error.name, 'GenericFailure',
        'error name is ' + request.error.name);
    runNextTest();
  };
}

let tests = [
  testPinChangeFailed,
  testPinChangeSuccess,
  testPinCardLockRetryCount,
  testPukCardLockRetryCount,
  testInvalidCardLockRetryCount
];

function runNextTest() {
  let test = tests.shift();
  if (!test) {
    cleanUp();
    return;
  }

  test();
}

function cleanUp() {
  SpecialPowers.removePermission("mobileconnection", document);

  finish();
}

runNextTest();
