/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

let Cc = SpecialPowers.Cc;
let Ci = SpecialPowers.Ci;

// Get RadioIntefaceLayer interface.
let RIL = Cc["@mozilla.org/ril;1"].getService(Ci.nsIRadioInterfaceLayer);
ok(RIL);

// Check voicemail information accessible.
ok(RIL.voicemailInfo);
ok(RIL.voicemailInfo.number);
ok(RIL.voicemailInfo.displayName);
// These are the emulator's hard coded voicemail number and alphaId.
is(RIL.voicemailInfo.number, "+15552175049");
is(RIL.voicemailInfo.displayName, "Voicemail");

finish();
