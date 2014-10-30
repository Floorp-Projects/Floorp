/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "icc_header.js";

/* Test PIN code changes fail */
taskHelper.push(function testPinChangeFailed() {
  // The default pin is '0000' in emulator
  let request = icc.setCardLock(
    {lockType: "pin",
     pin: "1111",
     newPin: "0000"});

  ok(request instanceof DOMRequest,
     "request instanceof " + request.constructor);

  request.onerror = function onerror() {
    is(request.error.name, "IncorrectPassword");
    // The default pin retries is 3, failed once becomes to 2
    is(request.error.retryCount, 2);

    // Reset pin retries by passing correct pin code.
    let resetRequest = icc.setCardLock(
      {lockType: "pin",
       pin: "0000",
       newPin: "0000"});

    resetRequest.onsuccess = function onsuccess() {
      taskHelper.runNext();
    };

    resetRequest.onerror = function onerror() {
      ok(false, "Reset pin retries got error: " + request.error.name);
      taskHelper.runNext();
    };
  };
});

/* Test PIN code changes success */
taskHelper.push(function testPinChangeSuccess() {
  // The default pin is '0000' in emulator
  let request = icc.setCardLock(
    {lockType: "pin",
     pin: "0000",
     newPin: "0000"});

  ok(request instanceof DOMRequest,
     "request instanceof " + request.constructor);

  request.onerror = function onerror() {
    ok(false, "Should not fail, got error: " + request.error.name);

    taskHelper.runNext();
  };

  request.onsuccess = function onsuccess() {
    is(request.result.lockType, "pin");
    is(request.result.success, true);

    taskHelper.runNext();
  };
});

/* Read PIN-lock retry count */
taskHelper.push(function testPinCardLockRetryCount() {
  let request = icc.getCardLockRetryCount('pin');

  ok(request instanceof DOMRequest,
     'request instanceof ' + request.constructor);

  request.onsuccess = function onsuccess() {
    is(request.result.lockType, 'pin',
        'lockType is ' + request.result.lockType);
    ok(request.result.retryCount >= 0,
        'retryCount is ' + request.result.retryCount);
    taskHelper.runNext();
  };
  request.onerror = function onerror() {
    // The operation is optional any might not be supported for all
    // all locks. In this case, we generate 'NotSupportedError' for
    // the valid lock types.
    is(request.error.name, 'RequestNotSupported',
        'error name is ' + request.error.name);
    taskHelper.runNext();
  };
});

/* Read PUK-lock retry count */
taskHelper.push(function testPukCardLockRetryCount() {
  let request = icc.getCardLockRetryCount('puk');

  ok(request instanceof DOMRequest,
     'request instanceof ' + request.constructor);

  request.onsuccess = function onsuccess() {
    is(request.result.lockType, 'puk',
        'lockType is ' + request.result.lockType);
    ok(request.result.retryCount >= 0,
        'retryCount is ' + request.result.retryCount);
    taskHelper.runNext();
  };
  request.onerror = function onerror() {
    // The operation is optional any might not be supported for all
    // all locks. In this case, we generate 'NotSupportedError' for
    // the valid lock types.
    is(request.error.name, 'RequestNotSupported',
        'error name is ' + request.error.name);
    taskHelper.runNext();
  };
});

/* Read lock retry count for an invalid entries  */
taskHelper.push(function testInvalidCardLockRetryCount() {
  let request = icc.getCardLockRetryCount('invalid-lock-type');

  ok(request instanceof DOMRequest,
     'request instanceof ' + request.constructor);

  request.onsuccess = function onsuccess() {
    ok(false,
        'request should never return success for an invalid lock type');
    taskHelper.runNext();
  };
  request.onerror = function onerror() {
    is(request.error.name, 'GenericFailure',
        'error name is ' + request.error.name);
    taskHelper.runNext();
  };
});

// Start test
taskHelper.runNext();
