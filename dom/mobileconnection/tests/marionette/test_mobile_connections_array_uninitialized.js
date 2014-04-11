/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

startTestCommon(function() {
  let connections =
    workingFrame.contentWindow.navigator.mozMobileConnections;

  let num = SpecialPowers.getIntPref("ril.numRadioInterfaces");
  is(connections.length, num, "ril.numRadioInterfaces");
});
