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

function checkSpeakerEnabled(phoneStatePrev, phoneStateNew, toggle, setSpeaker) {
  if (!audioManager) {
    audioManager = SpecialPowers.Cc[AUDIO_MANAGER_CONTRACT_ID]
      .getService(SpecialPowers.Ci.nsIAudioManager);
    ok(audioManager, "nsIAudioManager instance");
  }

  is(audioManager.phoneState, phoneStatePrev, "audioManager.phoneState");
  if (toggle) {
    telephony.speakerEnabled = setSpeaker;
  }
  is(audioManager.phoneState, phoneStateNew, "audioManager.phoneState");
}

// Start the test
startTest(function() {
  let inNumber  = "5555550201";
  let inCall;

  Promise.resolve()
    .then(() => checkSpeakerEnabled(PHONE_STATE_NORMAL, PHONE_STATE_NORMAL, false, false))
    // Dial in
    .then(() => gRemoteDial(inNumber))
    .then(call => { inCall = call; })
    .then(() => checkSpeakerEnabled(PHONE_STATE_RINGTONE, PHONE_STATE_RINGTONE, false, false))
    .then(() => gAnswer(inCall))
    .then(() => checkSpeakerEnabled(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL, false, false))
    // Go on Speaker (Don't go off speaker before hanging up.  This is to check
    // the condition for bug 1021550)
    .then(() => checkSpeakerEnabled(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL, true, true))
    // Hang up all
    .then(() => gRemoteHangUp(inCall))
    .then(() => checkSpeakerEnabled(PHONE_STATE_NORMAL, PHONE_STATE_NORMAL, false, false))
    // Make a second inbound call
    .then(() => gRemoteDial(inNumber))
    .then(call => { inCall = call; })
    .then(() => checkSpeakerEnabled(PHONE_STATE_RINGTONE, PHONE_STATE_RINGTONE, false, false))
    .then(() => gAnswer(inCall))
    .then(() => checkSpeakerEnabled(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL, false, false))
    // Hang up the call
    .then(() => gRemoteHangUp(inCall))
    .then(() => checkSpeakerEnabled(PHONE_STATE_NORMAL, PHONE_STATE_NORMAL, false, false))
    // End
    .then(null, error => {
      ok(false, 'promise rejects during test.');
    })
    .then(finish);
});
