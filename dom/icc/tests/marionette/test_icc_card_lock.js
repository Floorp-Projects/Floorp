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

    resetPinRetries("0000", runNextTest);
  };
}

/* Test PIN code changes fail notification */
function testPinChangeFailedNotification() {
  icc.addEventListener("icccardlockerror", function onicccardlockerror(result) {
    icc.removeEventListener("icccardlockerror", onicccardlockerror);

    is(result.lockType, "pin");
    // The default pin retries is 3, failed once becomes to 2
    is(result.retryCount, 2);

    resetPinRetries("0000", runNextTest);
  });

  // The default pin is '0000' in emulator
  let request = icc.setCardLock(
    {lockType: "pin",
     pin: "1111",
     newPin: "0000"});

  ok(request instanceof DOMRequest,
     "request instanceof " + request.constructor);
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

let tests = [
  testPinChangeFailed,
  testPinChangeFailedNotification,
  testPinChangeSuccess,
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
