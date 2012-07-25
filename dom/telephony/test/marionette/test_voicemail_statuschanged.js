/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const WHITELIST_PREF = "dom.voicemail.whitelist";
let uriPrePath = window.location.protocol + "//" + window.location.host;
SpecialPowers.setCharPref(WHITELIST_PREF, uriPrePath);

let voicemail = window.navigator.mozVoicemail;
ok(voicemail instanceof MozVoicemail);
is(voicemail.status, null);

function sendIndicatorPDU(pdu, listener, nextTest) {
  let smsCommand = "sms pdu " + pdu;
  let commandCompleted = false;
  let sawEvent = false;

  voicemail.addEventListener("statuschanged", function statusChanged(event) {
    voicemail.removeEventListener("statuschanged", statusChanged);

    try {
      listener(event);
    } catch (e) {
      ok(false, String(e));
    }

    sawEvent = true;
    if (commandCompleted) {
      nextTest();
    }
  });

  log("-> " + smsCommand);
  runEmulatorCmd(smsCommand, function(result) {
    log("<- " + result);
    is(result[0], "OK");
    commandCompleted = true;
    if (sawEvent) {
      nextTest();
    }
  });
}

// TODO: Add tests for store/discard once they are implemented
// See RadioInterfaceLayer.js / Bug #768441

function isVoicemailStatus(status) {
  is(voicemail.status.hasMessages, status.hasMessages);
  is(voicemail.status.messageCount, status.messageCount);
  is(voicemail.status.returnNumber, status.returnNumber);
  is(voicemail.status.returnMessage, status.returnMessage);
}

const MWI_PDU_PREFIX = "0000";
const MWI_PDU_UDH_PREFIX = "0040";
const MWI_PID_DEFAULT = "00";
const MWI_PID_RETURN_CALL_MSG = "5F";
const MWI_DCS_DATA_MSG = "F0";
const MWI_DCS_DISCARD_INACTIVE = "C0";
const MWI_DCS_DISCARD_ACTIVE = "C8";
const MWI_TIMESTAMP = "00000000000000";

const MWI_LEVEL1_SENDER = "+15125551234";
const MWI_LEVEL1_PDU_ADDRESS = PDUBuilder.buildAddress(MWI_LEVEL1_SENDER);
const MWI_DEFAULT_BODY = "1 new voicemail";
const MWI_UD_DEFAULT = PDUBuilder.buildUserData({
  body: MWI_DEFAULT_BODY
});

// Level 1 Message Waiting is just a return call message
const MWI_LEVEL1_PDU =
  MWI_PDU_PREFIX +
  MWI_LEVEL1_PDU_ADDRESS +
  MWI_PID_RETURN_CALL_MSG +
  MWI_DCS_DATA_MSG +
  MWI_TIMESTAMP +
  MWI_UD_DEFAULT;

function testLevel1Indicator() {

  function onLevel1Indicator(event) {
    let status = event.status;
    ok(status instanceof MozVoicemailStatus);
    is(status.hasMessages, true);
    is(status.messageCount, status.MESSAGE_COUNT_UNKNOWN);
    is(status.returnNumber, MWI_LEVEL1_SENDER);
    is(status.returnMessage, MWI_DEFAULT_BODY);
    isVoicemailStatus(status);
  }

  sendIndicatorPDU(MWI_LEVEL1_PDU, onLevel1Indicator, testLevel2DiscardActive);
}

const MWI_LEVEL2_SENDER = "+15125551235";
const MWI_LEVEL2_PDU_ADDRESS = PDUBuilder.buildAddress(MWI_LEVEL2_SENDER);
const MWI_LEVEL2_DISCARD_ACTIVE_PDU =
  MWI_PDU_PREFIX +
  MWI_LEVEL2_PDU_ADDRESS +
  MWI_PID_DEFAULT +
  MWI_DCS_DISCARD_ACTIVE +
  MWI_TIMESTAMP +
  MWI_UD_DEFAULT;

function testLevel2DiscardActive() {

  function onLevel2Active(event) {
    let status = event.status;
    ok(status instanceof MozVoicemailStatus);
    is(status.hasMessages, true);
    is(status.messageCount, status.MESSAGE_COUNT_UNKNOWN);
    is(status.returnNumber, MWI_LEVEL2_SENDER);
    is(status.returnMessage, MWI_DEFAULT_BODY);
    isVoicemailStatus(status);
  }

  sendIndicatorPDU(MWI_LEVEL2_DISCARD_ACTIVE_PDU,
                   onLevel2Active,
                   testLevel2DiscardInactive);

}

const MWI_LEVEL2_DISCARD_INACTIVE_PDU =
  MWI_PDU_PREFIX +
  MWI_LEVEL2_PDU_ADDRESS +
  MWI_PID_DEFAULT +
  MWI_DCS_DISCARD_INACTIVE +
  MWI_TIMESTAMP +
  MWI_UD_DEFAULT;

function testLevel2DiscardInactive() {
  function onLevel2Inactive(event) {
    let status = event.status;
    ok(status instanceof MozVoicemailStatus);
    is(status.hasMessages, false);
    is(status.messageCount, 0);
    is(status.returnNumber, MWI_LEVEL2_SENDER);
    is(status.returnMessage, MWI_DEFAULT_BODY);
    isVoicemailStatus(status);
  }

  sendIndicatorPDU(MWI_LEVEL2_DISCARD_INACTIVE_PDU,
                   onLevel2Inactive,
                   testLevel3DiscardActive);
}


// Tests for Level 3 MWI with a message count in the User Data Header
const MWI_LEVEL3_SENDER = "+15125551236";
const MWI_LEVEL3_PDU_ADDRESS = PDUBuilder.buildAddress(MWI_LEVEL3_SENDER);

const MWI_LEVEL3_ACTIVE_UDH_MSG_COUNT = 3;
const MWI_LEVEL3_ACTIVE_BODY = "3 new voicemails";
const MWI_LEVEL3_ACTIVE_UD = PDUBuilder.buildUserData({
  headers: [{
    id: RIL.PDU_IEI_SPECIAL_SMS_MESSAGE_INDICATION,
    length: 2,
    octets: [
      RIL.PDU_MWI_STORE_TYPE_DISCARD,
      MWI_LEVEL3_ACTIVE_UDH_MSG_COUNT
    ]
  }],
  body: MWI_LEVEL3_ACTIVE_BODY
});

const MWI_LEVEL3_DISCARD_ACTIVE_PDU =
  MWI_PDU_UDH_PREFIX +
  MWI_LEVEL3_PDU_ADDRESS +
  MWI_PID_DEFAULT +
  MWI_DCS_DISCARD_ACTIVE +
  MWI_TIMESTAMP +
  MWI_LEVEL3_ACTIVE_UD;

function testLevel3DiscardActive() {

  function onLevel3Active(event) {
    let status = event.status;
    ok(status instanceof MozVoicemailStatus);
    is(status.hasMessages, true);
    is(status.messageCount, MWI_LEVEL3_ACTIVE_UDH_MSG_COUNT);
    is(status.returnNumber, MWI_LEVEL3_SENDER);
    is(status.returnMessage, MWI_LEVEL3_ACTIVE_BODY);
    isVoicemailStatus(status);
  }

  sendIndicatorPDU(MWI_LEVEL3_DISCARD_ACTIVE_PDU,
                   onLevel3Active,
                   testLevel3DiscardInactive);
}

const MWI_LEVEL3_INACTIVE_BODY = "No unread voicemails";
const MWI_LEVEL3_INACTIVE_UD = PDUBuilder.buildUserData({
  headers: [{
    id: RIL.PDU_IEI_SPECIAL_SMS_MESSAGE_INDICATION,
    length: 2,
    octets: [
      RIL.PDU_MWI_STORE_TYPE_DISCARD,
      0 // messageCount
    ]
  }],
  body: MWI_LEVEL3_INACTIVE_BODY
});

const MWI_LEVEL3_DISCARD_INACTIVE_PDU =
  MWI_PDU_UDH_PREFIX +
  MWI_LEVEL3_PDU_ADDRESS +
  MWI_PID_DEFAULT +
  MWI_DCS_DISCARD_ACTIVE +
  MWI_TIMESTAMP +
  MWI_LEVEL3_INACTIVE_UD;

function testLevel3DiscardInactive() {
  function onLevel3Inactive(event) {
    let status = event.status;
    ok(status instanceof MozVoicemailStatus);
    is(status.hasMessages, false);
    is(status.messageCount, 0);
    is(status.returnNumber, MWI_LEVEL3_SENDER);
    is(status.returnMessage, MWI_LEVEL3_INACTIVE_BODY);
    isVoicemailStatus(status);
  }

  sendIndicatorPDU(MWI_LEVEL3_DISCARD_INACTIVE_PDU, onLevel3Inactive, cleanUp);
}

function cleanUp() {
  SpecialPowers.clearUserPref(WHITELIST_PREF);
  finish();
}

testLevel1Indicator();
