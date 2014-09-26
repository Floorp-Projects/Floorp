/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const MARIONETTE_TIMEOUT = 60000;
const MARIONETTE_HEAD_JS = 'head.js';

startTestCommon(function() {
  let serviceId = 0;

  // These are the emulator's hard coded voicemail number and alphaId
  is(voicemail.getNumber(serviceId), "+15552175049");
  is(voicemail.getDisplayName(serviceId), "Voicemail");

  is(voicemail.getNumber(), "+15552175049");
  is(voicemail.getDisplayName(), "Voicemail");
});
