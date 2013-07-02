/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

let Cc = SpecialPowers.Cc;
let Ci = SpecialPowers.Ci;

// Get RadioInterfaceLayer interface.
let radioInterfaceLayer =
  Cc["@mozilla.org/ril;1"].getService(Ci.nsIRadioInterfaceLayer);
ok(radioInterfaceLayer);

// Get RadioInterface.
let radioInterface = radioInterfaceLayer.getRadioInterface(0);
ok(radioInterface);

// Check voicemail information accessible.
ok(radioInterface.voicemailInfo);
ok(radioInterface.voicemailInfo.number);
ok(radioInterface.voicemailInfo.displayName);
// These are the emulator's hard coded voicemail number and alphaId.
is(radioInterface.voicemailInfo.number, "+15552175049");
is(radioInterface.voicemailInfo.displayName, "Voicemail");

finish();
