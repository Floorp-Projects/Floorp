/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const AUDIO_MANAGER_CONTRACT_ID = "@mozilla.org/telephony/audiomanager;1";

// See nsIAudioManager
const PHONE_STATE_INVALID          = -2;
const PHONE_STATE_CURRENT          = -1;
const PHONE_STATE_NORMAL           = 0;
const PHONE_STATE_RINGTONE         = 1;
const PHONE_STATE_IN_CALL          = 2;
const PHONE_STATE_IN_COMMUNICATION = 3;

let audioManager;
function checkStates(speakerEnabled, phoneState) {
  if (!audioManager) {
    audioManager = SpecialPowers.Cc[AUDIO_MANAGER_CONTRACT_ID]
                                .getService(SpecialPowers.Ci.nsIAudioManager);
    ok(audioManager, "nsIAudioManager instance");
  }

  is(telephony.speakerEnabled, speakerEnabled, "telephony.speakerEnabled");
  if (phoneState == PHONE_STATE_CURRENT) {
    ok(audioManager.phoneState === PHONE_STATE_CURRENT ||
       audioManager.phoneState === PHONE_STATE_NORMAL, "audioManager.phoneState");
  } else {
    is(audioManager.phoneState, phoneState, "audioManager.phoneState");
  }
}

function check(phoneStateOrig, phoneStateEnabled, phoneStateDisabled) {
  checkStates(false, phoneStateOrig);

  let canEnableSpeaker = arguments.length > 1;
  telephony.speakerEnabled = true;
  if (canEnableSpeaker) {
    checkStates(true, phoneStateEnabled);
  } else {
    checkStates(false, phoneStateOrig);
    return;
  }

  telephony.speakerEnabled = false;
  checkStates(false, arguments.length > 2 ? phoneStateDisabled : phoneStateOrig);
}

// Start the test
startTest(function() {
  let outNumber = "5555550101";
  let inNumber  = "5555550201";
  let outCall;
  let inCall;

  Promise.resolve()
    .then(() => check(PHONE_STATE_CURRENT, PHONE_STATE_NORMAL, PHONE_STATE_NORMAL))

    // Dial in
    .then(() => gRemoteDial(inNumber))
    .then(call => { inCall = call; })
    .then(() => check(PHONE_STATE_RINGTONE, PHONE_STATE_RINGTONE, PHONE_STATE_RINGTONE))
    .then(() => gAnswer(inCall))
    .then(() => check(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL))
    // Hang up all
    .then(() => gRemoteHangUp(inCall))
    .then(() => check(PHONE_STATE_NORMAL, PHONE_STATE_NORMAL))

    // Dial out
    .then(() => gDial(outNumber))
    .then(call => { outCall = call; })
    .then(() => check(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL))
    .then(() => gRemoteAnswer(outCall))
    .then(() => check(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL))
    // Hang up all
    .then(() => gRemoteHangUp(outCall))
    .then(() => check(PHONE_STATE_NORMAL, PHONE_STATE_NORMAL))

    // Dial out
    .then(() => gDial(outNumber))
    .then(call => { outCall = call; })
    .then(() => check(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL))
    .then(() => gRemoteAnswer(outCall))
    .then(() => check(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL))
    .then(() => gHold(outCall))
    .then(() => check(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL))
    .then(() => gResume(outCall))
    .then(() => check(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL))
    // Dial out and dial in
    .then(() => gRemoteDial(inNumber))
    .then(call => { inCall = call; })
    .then(() => check(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL))
    .then(() => gAnswer(inCall))
    .then(() => check(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL))
    // Conference
    .then(() => gAddCallsToConference([outCall, inCall]))
    .then(() => check(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL))
    // Hang up all
    .then(() => gRemoteHangUpCalls([outCall, inCall]))
    .then(() => check(PHONE_STATE_NORMAL, PHONE_STATE_NORMAL))

    // End
    .then(null, error => {
      ok(false, 'promise rejects during test.');
    })
    .then(finish);
});
