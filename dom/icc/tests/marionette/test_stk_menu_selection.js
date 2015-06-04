/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "head.js";


// Start tests
startTestCommon(function() {
  let icc = getMozIcc();

  // APDU format of ENVELOPE:
  // Class = 'A0', INS = 'C2', P1 = '00', P2 = '00', XXXX, (No Le)

  // Since |sendStkMenuSelection| is an API without call back to identify the
  // result, the tests of |sendStkMenuSelection| must be executed one by one with
  // |verifyWithPeekedStkEnvelope| introduced here.
  return Promise.resolve()
    .then(() => icc.sendStkMenuSelection(1, true))
    .then(() => verifyWithPeekedStkEnvelope(
      "D3" + // BER_MENU_SELECTION_TAG
      "09" + // Length
      "82020181" + // TAG_DEVICE_ID (STK_DEVICE_ID_KEYPAD, STK_DEVICE_ID_SIM)
      "900101" + // TAG_ITEM_ID (Item (1))
      "9500" // TAG_HELP_REQUEST
    ))

    .then(() => icc.sendStkMenuSelection(0, false))
    .then(() => verifyWithPeekedStkEnvelope(
      "D3" + // BER_MENU_SELECTION_TAG
      "07" + // Length
      "82020181" + // TAG_DEVICE_ID (STK_DEVICE_ID_KEYPAD, STK_DEVICE_ID_SIM)
      "900100" // TAG_ITEM_ID (Item (0))
    ));
});