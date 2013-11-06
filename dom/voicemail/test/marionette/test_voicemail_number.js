/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

SpecialPowers.addPermission("voicemail", true, document);

let voicemail = window.navigator.mozVoicemail;
ok(voicemail instanceof MozVoicemail);

// These are the emulator's hard coded voicemail number and alphaId
is(voicemail.number, "+15552175049");
is(voicemail.displayName, "Voicemail");

SpecialPowers.removePermission("voicemail", document);
finish();
