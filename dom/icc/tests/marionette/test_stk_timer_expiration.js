/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 20000;
MARIONETTE_HEAD_JS = "head.js";


// Start tests
startTestCommon(function() {
  let icc = getMozIcc();

  // APDU format of ENVELOPE:
  // Class = 'A0', INS = 'C2', P1 = '00', P2 = '00', XXXX, (No Le)

  // Since |sendStkTimerExpiration| is an API without call back to identify the
  // result, the tests of |sendStkMenuSelection| must be executed one by one with
  // |verifyWithPeekedStkEnvelope| introduced here.
  return Promise.resolve()
    .then(() => icc.sendStkTimerExpiration({ timerId: 5, timerValue: 1234567 / 1000 }))
    .then(() => verifyWithPeekedStkEnvelope(
      "D7" + // BER_TIMER_EXPIRATION_TAG
      "0C" + // Length
      "82028281" + // TAG_DEVICE_ID (STK_DEVICE_ID_ME, STK_DEVICE_ID_SIM)
      "A40105" + // TAG_TIMER_IDENTIFIER (5)
      "A503000243" // TIMER_VALUE (00:20:34) = 1234 seconds
    ));
});