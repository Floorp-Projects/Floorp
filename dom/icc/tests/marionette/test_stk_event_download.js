/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 90000;
MARIONETTE_HEAD_JS = "head.js";


// Start tests
startTestCommon(function() {
  let icc = getMozIcc();

  // APDU format of ENVELOPE:
  // Class = 'A0', INS = 'C2', P1 = '00', P2 = '00', XXXX, (No Le)

  // Since |sendStkEventDownload| is an API without call back to identify the
  // result, the tests of |sendStkMenuSelection| must be executed one by one with
  // |verifyWithPeekedStkEnvelope| introduced here.
  return Promise.resolve()
    .then(() => icc.sendStkEventDownload({
      eventType: MozIccManager.STK_EVENT_TYPE_USER_ACTIVITY
    }))
    .then(() => verifyWithPeekedStkEnvelope(
      "D6" + // BER_EVENT_DOWNLOAD_TAG
      "07" + // Length
      "990104" + // TAG_EVENT_LIST (STK_EVENT_TYPE_USER_ACTIVITY)
      "82028281" // TAG_DEVICE_ID (STK_DEVICE_ID_ME, STK_DEVICE_ID_SIM)
    ))

    .then(() => icc.sendStkEventDownload({
      eventType: MozIccManager.STK_EVENT_TYPE_IDLE_SCREEN_AVAILABLE
    }))
    .then(() => verifyWithPeekedStkEnvelope(
      "D6" + // BER_EVENT_DOWNLOAD_TAG
      "07" + // Length
      "990105" + // TAG_EVENT_LIST (STK_EVENT_TYPE_IDLE_SCREEN_AVAILABLE)
      "82020281" // TAG_DEVICE_ID (STK_DEVICE_ID_DISPLAY, STK_DEVICE_ID_SIM)
    ))

    .then(() => icc.sendStkEventDownload({
      eventType: MozIccManager.STK_EVENT_TYPE_LOCATION_STATUS,
      locationStatus: MozIccManager.STK_SERVICE_STATE_NORMAL,
      locationInfo: {
        mcc: "466",
        mnc: "92",
        gsmLocationAreaCode: 10291,
        gsmCellId: 19072823
      }
    }))
    .then(() => verifyWithPeekedStkEnvelope(
      "D6" + // BER_EVENT_DOWNLOAD_TAG
      "15" + // Length
      "990103" + // TAG_EVENT_LIST (STK_EVENT_TYPE_LOCATION_STATUS)
      "82028281" + // TAG_DEVICE_ID (STK_DEVICE_ID_ME, STK_DEVICE_ID_SIM)
      "9B0100" + // TAG_LOCATION_STATUS (STK_SERVICE_STATE_NORMAL)
      "930964F629283301230737" // TAG_LOCATION_INFO (mccmnc = 46692, lac = 10291, cellId = 19072823)
    ))

    .then(() => icc.sendStkEventDownload({
      eventType: MozIccManager.STK_EVENT_TYPE_LOCATION_STATUS,
      locationStatus: MozIccManager.STK_SERVICE_STATE_LIMITED,
      // locationInfo shall be ignored if locationStatus != STK_SERVICE_STATE_NORMAL
      locationInfo: {
        mcc: "466",
        mnc: "92",
        gsmLocationAreaCode: 10291,
        gsmCellId: 19072823
      }
    }))
    .then(() => verifyWithPeekedStkEnvelope(
      "D6" + // BER_EVENT_DOWNLOAD_TAG
      "0A" + // Length
      "990103" + // TAG_EVENT_LIST (STK_EVENT_TYPE_LOCATION_STATUS)
      "82028281" + // TAG_DEVICE_ID (STK_DEVICE_ID_ME, STK_DEVICE_ID_SIM)
      "9B0101" // TAG_LOCATION_STATUS (STK_SERVICE_STATE_LIMITED)
    ))

    .then(() => icc.sendStkEventDownload({
      eventType: MozIccManager.STK_EVENT_TYPE_LOCATION_STATUS,
      locationStatus: MozIccManager.STK_SERVICE_STATE_UNAVAILABLE,
    }))
    .then(() => verifyWithPeekedStkEnvelope(
      "D6" + // BER_EVENT_DOWNLOAD_TAG
      "0A" + // Length
      "990103" + // TAG_EVENT_LIST (STK_EVENT_TYPE_LOCATION_STATUS)
      "82028281" + // TAG_DEVICE_ID (STK_DEVICE_ID_ME, STK_DEVICE_ID_SIM)
      "9B0102" // TAG_LOCATION_STATUS (STK_SERVICE_STATE_UNAVAILABLE)
    ))

    .then(() => icc.sendStkEventDownload({
      eventType: MozIccManager.STK_EVENT_TYPE_MT_CALL,
      number: "+9876543210", // International number
      isIssuedByRemote: true,
      error: null
    }))
    .then(() => verifyWithPeekedStkEnvelope(
      "D6" + // BER_EVENT_DOWNLOAD_TAG
      "12" + // Length
      "990100" + // TAG_EVENT_LIST (STK_EVENT_TYPE_MT_CALL)
      "82028381" + // TAG_DEVICE_ID (STK_DEVICE_ID_NETWORK, STK_DEVICE_ID_SIM)
      "9C0100" + // TAG_TRANSACTION_ID (transactionId always set to 0)
      "8606918967452301" // TAG_ADDRESS (+9876543210)
    ))

    .then(() => icc.sendStkEventDownload({
      eventType: MozIccManager.STK_EVENT_TYPE_MT_CALL,
      number: "987654321", // National number
      isIssuedByRemote: true,
      error: null
    }))
    .then(() => verifyWithPeekedStkEnvelope(
      "D6" + // BER_EVENT_DOWNLOAD_TAG
      "12" + // Length
      "990100" + // TAG_EVENT_LIST (STK_EVENT_TYPE_MT_CALL)
      "82028381" + // TAG_DEVICE_ID (STK_DEVICE_ID_NETWORK, STK_DEVICE_ID_SIM)
      "9C0100" + // TAG_TRANSACTION_ID (transactionId always set to 0)
      "86068189674523F1" // TAG_ADDRESS (987654321)
    ))

    .then(() => icc.sendStkEventDownload({
      eventType: MozIccManager.STK_EVENT_TYPE_CALL_CONNECTED,
      isIssuedByRemote: true,
    }))
    .then(() => verifyWithPeekedStkEnvelope(
      "D6" + // BER_EVENT_DOWNLOAD_TAG
      "0A" + // Length
      "990101" + // TAG_EVENT_LIST (STK_EVENT_TYPE_CALL_CONNECTED)
      "82028381" + // TAG_DEVICE_ID (STK_DEVICE_ID_NETWORK, STK_DEVICE_ID_SIM)
      "9C0100" // TAG_TRANSACTION_ID (transactionId always set to 0)
    ))

    .then(() => icc.sendStkEventDownload({
      eventType: MozIccManager.STK_EVENT_TYPE_CALL_CONNECTED,
      isIssuedByRemote: false,
    }))
    .then(() => verifyWithPeekedStkEnvelope(
      "D6" + // BER_EVENT_DOWNLOAD_TAG
      "0A" + // Length
      "990101" + // TAG_EVENT_LIST (STK_EVENT_TYPE_CALL_CONNECTED)
      "82028281" + // TAG_DEVICE_ID (STK_DEVICE_ID_ME, STK_DEVICE_ID_SIM)
      "9C0100" // TAG_TRANSACTION_ID (transactionId always set to 0)
    ))

    .then(() => icc.sendStkEventDownload({
      eventType: MozIccManager.STK_EVENT_TYPE_CALL_DISCONNECTED,
      isIssuedByRemote: false,
      error: "BusyError"
    }))
    .then(() => verifyWithPeekedStkEnvelope(
      "D6" + // BER_EVENT_DOWNLOAD_TAG
      "0E" + // Length
      "990102" + // TAG_EVENT_LIST (STK_EVENT_TYPE_CALL_DISCONNECTED)
      "82028281" + // TAG_DEVICE_ID (STK_DEVICE_ID_ME, STK_DEVICE_ID_SIM)
      "9C0100" + // TAG_TRANSACTION_ID (transactionId always set to 0)
      "9A026091" // TAG_CAUSE (Busy)
    ))

    .then(() => icc.sendStkEventDownload({
      eventType: MozIccManager.STK_EVENT_TYPE_CALL_DISCONNECTED,
      isIssuedByRemote: true,
    }))
    .then(() => verifyWithPeekedStkEnvelope(
      "D6" + // BER_EVENT_DOWNLOAD_TAG
      "0A" + // Length
      "990102" + // TAG_EVENT_LIST (STK_EVENT_TYPE_CALL_DISCONNECTED)
      "82028381" + // TAG_DEVICE_ID (STK_DEVICE_ID_NETWORK, STK_DEVICE_ID_SIM)
      "9C0100" // TAG_TRANSACTION_ID (transactionId always set to 0)
    ))

    .then(() => icc.sendStkEventDownload({
      eventType: MozIccManager.STK_EVENT_TYPE_LANGUAGE_SELECTION,
      language: "zh",
    }))
    .then(() => verifyWithPeekedStkEnvelope(
      "D6" + // BER_EVENT_DOWNLOAD_TAG
      "0B" + // Length
      "990107" + // TAG_EVENT_LIST (STK_EVENT_TYPE_LANGUAGE_SELECTION)
      "82028281" + // TAG_DEVICE_ID (STK_DEVICE_ID_ME, STK_DEVICE_ID_SIM)
      "2D027A68" // TAG_LANGUAGE ("zh")
    ))

    .then(() => icc.sendStkEventDownload({
      eventType: MozIccManager.STK_EVENT_TYPE_BROWSER_TERMINATION,
      terminationCause: MozIccManager.STK_BROWSER_TERMINATION_CAUSE_USER,
    }))
    .then(() => verifyWithPeekedStkEnvelope(
      "D6" + // BER_EVENT_DOWNLOAD_TAG
      "0A" + // Length
      "990108" + // TAG_EVENT_LIST (STK_EVENT_TYPE_BROWSER_TERMINATION)
      "82028281" + // TAG_DEVICE_ID (STK_DEVICE_ID_ME, STK_DEVICE_ID_SIM)
      "B40100" // TAG_BROWSER_TERMINATION_CAUSE (USER)
    ))

    .then(() => icc.sendStkEventDownload({
      eventType: MozIccManager.STK_EVENT_TYPE_BROWSER_TERMINATION,
      terminationCause: MozIccManager.STK_BROWSER_TERMINATION_CAUSE_ERROR,
    }))
    .then(() => verifyWithPeekedStkEnvelope(
      "D6" + // BER_EVENT_DOWNLOAD_TAG
      "0A" + // Length
      "990108" + // TAG_EVENT_LIST (STK_EVENT_TYPE_BROWSER_TERMINATION)
      "82028281" + // TAG_DEVICE_ID (STK_DEVICE_ID_ME, STK_DEVICE_ID_SIM)
      "B40101" // TAG_BROWSER_TERMINATION_CAUSE (ERROR)
    ));
});