/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {Cc: Cc, Ci: Ci, Cr: Cr, Cu: Cu} = SpecialPowers;

let Promise = Cu.import("resource://gre/modules/Promise.jsm").Promise;

const PDU_DCS_CODING_GROUP_BITS          = 0xF0;
const PDU_DCS_MSG_CODING_7BITS_ALPHABET  = 0x00;
const PDU_DCS_MSG_CODING_8BITS_ALPHABET  = 0x04;
const PDU_DCS_MSG_CODING_16BITS_ALPHABET = 0x08;

const PDU_DCS_MSG_CLASS_BITS             = 0x03;
const PDU_DCS_MSG_CLASS_NORMAL           = 0xFF;
const PDU_DCS_MSG_CLASS_0                = 0x00;
const PDU_DCS_MSG_CLASS_ME_SPECIFIC      = 0x01;
const PDU_DCS_MSG_CLASS_SIM_SPECIFIC     = 0x02;
const PDU_DCS_MSG_CLASS_TE_SPECIFIC      = 0x03;
const PDU_DCS_MSG_CLASS_USER_1           = 0x04;
const PDU_DCS_MSG_CLASS_USER_2           = 0x05;

const GECKO_SMS_MESSAGE_CLASSES = {};
GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]       = "normal";
GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_0]            = "class-0";
GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_ME_SPECIFIC]  = "class-1";
GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_SIM_SPECIFIC] = "class-2";
GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_TE_SPECIFIC]  = "class-3";
GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_USER_1]       = "user-1";
GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_USER_2]       = "user-2";

const CB_MESSAGE_SIZE_GSM  = 88;
const CB_MESSAGE_SIZE_ETWS = 56;

const CB_GSM_MESSAGEID_ETWS_BEGIN = 0x1100;
const CB_GSM_MESSAGEID_ETWS_END   = 0x1107;

const CB_GSM_GEOGRAPHICAL_SCOPE_NAMES = [
  "cell-immediate",
  "plmn",
  "location-area",
  "cell"
];

const CB_ETWS_WARNING_TYPE_NAMES = [
  "earthquake",
  "tsunami",
  "earthquake-tsunami",
  "test",
  "other"
];

const CB_DCS_LANG_GROUP_1 = [
  "de", "en", "it", "fr", "es", "nl", "sv", "da", "pt", "fi",
  "no", "el", "tr", "hu", "pl", null
];
const CB_DCS_LANG_GROUP_2 = [
  "cs", "he", "ar", "ru", "is", null, null, null, null, null,
  null, null, null, null, null, null
];

const CB_MAX_CONTENT_PER_PAGE_7BIT = Math.floor((CB_MESSAGE_SIZE_GSM - 6) * 8 / 7);
const CB_MAX_CONTENT_PER_PAGE_UCS2 = Math.floor((CB_MESSAGE_SIZE_GSM - 6) / 2);

const DUMMY_BODY_7BITS = "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
                       + "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
                       + "@@@@@@@@@@@@@"; // 93 ascii chars.
const DUMMY_BODY_7BITS_IND = DUMMY_BODY_7BITS.substr(3);
const DUMMY_BODY_UCS2 = "\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000"
                      + "\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000"
                      + "\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000"
                      + "\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000"
                      + "\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000"
                      + "\u0000"; // 41 unicode chars.
const DUMMY_BODY_UCS2_IND = DUMMY_BODY_UCS2.substr(1);

is(DUMMY_BODY_7BITS.length,     CB_MAX_CONTENT_PER_PAGE_7BIT,     "DUMMY_BODY_7BITS.length");
is(DUMMY_BODY_7BITS_IND.length, CB_MAX_CONTENT_PER_PAGE_7BIT - 3, "DUMMY_BODY_7BITS_IND.length");
is(DUMMY_BODY_UCS2.length,      CB_MAX_CONTENT_PER_PAGE_UCS2,     "DUMMY_BODY_UCS2.length");
is(DUMMY_BODY_UCS2_IND.length,  CB_MAX_CONTENT_PER_PAGE_UCS2 - 1, "DUMMY_BODY_UCS2_IND.length");

/**
 * Compose input number into specified number of semi-octets.
 *
 * @param: aNum
 *         The number to be converted.
 * @param: aNumSemiOctets
 *         Number of semi-octects to be composed to.
 *
 * @return The composed Hex String.
 */
function buildHexStr(aNum, aNumSemiOctets) {
  let str = aNum.toString(16);
  ok(str.length <= aNumSemiOctets);
  while (str.length < aNumSemiOctets) {
    str = "0" + str;
  }
  return str;
}

/**
 * Helper function to decode the given DCS into encoding type, language,
 * language indicator and message class.
 *
 * @param: aDcs
 *         The DCS to be decoded.
 *
 * @return [encoding, language, hasLanguageIndicator,
 *          GECKO_SMS_MESSAGE_CLASSES[messageClass]]
 */
function decodeGsmDataCodingScheme(aDcs) {
  let language = null;
  let hasLanguageIndicator = false;
  let encoding = PDU_DCS_MSG_CODING_7BITS_ALPHABET;
  let messageClass = PDU_DCS_MSG_CLASS_NORMAL;

  switch (aDcs & PDU_DCS_CODING_GROUP_BITS) {
    case 0x00: // 0000
      language = CB_DCS_LANG_GROUP_1[aDcs & 0x0F];
      break;

    case 0x10: // 0001
      switch (aDcs & 0x0F) {
        case 0x00:
          hasLanguageIndicator = true;
          break;
        case 0x01:
          encoding = PDU_DCS_MSG_CODING_16BITS_ALPHABET;
          hasLanguageIndicator = true;
          break;
      }
      break;

    case 0x20: // 0010
      language = CB_DCS_LANG_GROUP_2[aDcs & 0x0F];
      break;

    case 0x40: // 01xx
    case 0x50:
    //case 0x60:
    //case 0x70:
    case 0x90: // 1001
      encoding = (aDcs & 0x0C);
      if (encoding == 0x0C) {
        encoding = PDU_DCS_MSG_CODING_7BITS_ALPHABET;
      }
      messageClass = (aDcs & PDU_DCS_MSG_CLASS_BITS);
      break;

    case 0xF0:
      encoding = (aDcs & 0x04) ? PDU_DCS_MSG_CODING_8BITS_ALPHABET
                              : PDU_DCS_MSG_CODING_7BITS_ALPHABET;
      switch(aDcs & PDU_DCS_MSG_CLASS_BITS) {
        case 0x01: messageClass = PDU_DCS_MSG_CLASS_USER_1; break;
        case 0x02: messageClass = PDU_DCS_MSG_CLASS_USER_2; break;
        case 0x03: messageClass = PDU_DCS_MSG_CLASS_TE_SPECIFIC; break;
      }
      break;

    case 0x30: // 0011 (Reserved)
    case 0x80: // 1000 (Reserved)
    case 0xA0: // 1010..1100 (Reserved)
    case 0xB0:
    case 0xC0:
      break;
    default:
      throw new Error("Unsupported CBS data coding scheme: " + aDcs);
  }

  return [encoding, language, hasLanguageIndicator,
          GECKO_SMS_MESSAGE_CLASSES[messageClass]];
}

/**
 * Push required permissions and test if |navigator.mozCellBroadcast| exists.
 * Resolve if it does, reject otherwise.
 *
 * Fulfill params:
 *   cbManager -- an reference to navigator.mozCellBroadcast.
 *
 * Reject params: (none)
 *
 * @return A deferred promise.
 */
let cbManager;
function ensureCellBroadcast() {
  let deferred = Promise.defer();

  let permissions = [{
    "type": "cellbroadcast",
    "allow": 1,
    "context": document,
  }];
  SpecialPowers.pushPermissions(permissions, function() {
    ok(true, "permissions pushed: " + JSON.stringify(permissions));

    cbManager = window.navigator.mozCellBroadcast;
    if (cbManager) {
      log("navigator.mozCellBroadcast is instance of " + cbManager.constructor);
    } else {
      log("navigator.mozCellBroadcast is undefined.");
    }

    if (cbManager instanceof window.MozCellBroadcast) {
      deferred.resolve(cbManager);
    } else {
      deferred.reject();
    }
  });

  return deferred.promise;
}

/**
 * Send emulator command with safe guard.
 *
 * We should only call |finish()| after all emulator command transactions
 * end, so here comes with the pending counter.  Resolve when the emulator
 * gives positive response, and reject otherwise.
 *
 * Fulfill params:
 *   result -- an array of emulator response lines.
 *
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @return A deferred promise.
 */
let pendingEmulatorCmdCount = 0;
function runEmulatorCmdSafe(aCommand) {
  let deferred = Promise.defer();

  ++pendingEmulatorCmdCount;
  runEmulatorCmd(aCommand, function(aResult) {
    --pendingEmulatorCmdCount;

    ok(true, "Emulator response: " + JSON.stringify(aResult));
    if (Array.isArray(aResult) && aResult[aResult.length - 1] === "OK") {
      deferred.resolve(aResult);
    } else {
      deferred.reject(aResult);
    }
  });

  return deferred.promise;
}

/**
 * Send raw CBS PDU to emulator.
 *
 * @param: aPdu
 *         A hex string representing the whole CBS PDU.
 *
 * Fulfill params:
 *   result -- an array of emulator response lines.
 *
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @return A deferred promise.
 */
function sendRawCbsToEmulator(aPdu) {
  let command = "cbs pdu " + aPdu;
  return runEmulatorCmdSafe(command);
}

/**
 * Wait for one named Cellbroadcast event.
 *
 * Resolve if that named event occurs.  Never reject.
 *
 * Fulfill params: the DOMEvent passed.
 *
 * @param aEventName
 *        A string event name.
 *
 * @return A deferred promise.
 */
function waitForManagerEvent(aEventName) {
  let deferred = Promise.defer();

  cbManager.addEventListener(aEventName, function onevent(aEvent) {
    cbManager.removeEventListener(aEventName, onevent);

    ok(true, "Cellbroadcast event '" + aEventName + "' got.");
    deferred.resolve(aEvent);
  });

  return deferred.promise;
}

/**
 * Send multiple raw CB PDU to emulator and wait
 *
 * @param: aPdus
 *         A array of hex strings. Each represents a CB PDU.
 *         These PDUs are expected to be concatenated into single CB Message.
 *
 * Fulfill params:
 *   result -- array of resolved Promise, where
 *             result[0].message representing the received message.
 *             result[1-n] represents the response of sent emulator command.
 *
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @return A deferred promise.
 */
function sendMultipleRawCbsToEmulatorAndWait(aPdus) {
  let promises = [];

  promises.push(waitForManagerEvent("received"));
  for (let pdu of aPdus) {
    promises.push(sendRawCbsToEmulator(pdu));
  }

  return Promise.all(promises).then(aResults => aResults[0].message);
}

/**
 * Wait for pending emulator transactions and call |finish()|.
 */
function cleanUp() {
  // Use ok here so that we have at least one test run.
  ok(true, ":: CLEANING UP ::");

  waitFor(finish, function() {
    return pendingEmulatorCmdCount === 0;
  });
}

/**
 * Switch modem for receving upcoming emulator commands.
 *
 * @param: aServiceId
 *         The id of the modem to be switched to.
 *
 * Fulfill params:
 *   result -- an array of emulator response lines.
 *
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @return A deferred promise.
 */
function selectModem(aServiceId) {
  let command = "mux modem " + aServiceId;
  return runEmulatorCmdSafe(command);
}

/**
 * Helper to run the test case only needed in Multi-SIM environment.
 *
 * @param  aTest
 *         A function which will be invoked w/o parameter.
 * @return a Promise object.
 */
function runIfMultiSIM(aTest) {
  let numRIL;
  try {
    numRIL = SpecialPowers.getIntPref("ril.numRadioInterfaces");
  } catch (ex) {
    numRIL = 1;  // Pref not set.
  }

  if (numRIL > 1) {
    return aTest();
  } else {
    log("Not a Multi-SIM environment. Test is skipped.");
    return Promise.resolve();
  }
}

/**
 * Common test routine helper for cell broadcast tests.
 *
 * This function ensures global |cbManager| variable is available during the
 * process and performs clean-ups as well.
 *
 * @param aTestCaseMain
 *        A function that takes no parameter.
 */
function startTestCommon(aTestCaseMain) {
  Promise.resolve()
         .then(ensureCellBroadcast)
         .then(aTestCaseMain)
         .then(cleanUp, function() {
           ok(false, 'promise rejects during test.');
           cleanUp();
         });
}
