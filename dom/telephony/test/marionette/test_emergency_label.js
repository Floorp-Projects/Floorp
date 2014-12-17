/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const DEFAULT_ECC_LIST = "112,911";

function setEccListProperty(list) {
  log("Set property ril.ecclist: " + list);

  // We should wrap empty |list| by ''. Otherwise, the entire command will be
  // "setprop ril.ecclist" which causus the command error.
  if (!list) {
    list = "''";
  }

  return emulator.runShellCmd(["setprop","ril.ecclist", list])
    .then(list => list);
}

function getEccListProperty() {
  log("Get property ril.ecclist.");

  return emulator.runShellCmd(["getprop","ril.ecclist"])
    .then(aResult => {
      return !aResult.length ? "" : aResult[0];
    });
}

function testEmergencyLabel(number, list) {
  if (!list) {
    list = DEFAULT_ECC_LIST;
  }
  let index = list.split(",").indexOf(number);
  let emergency = index != -1;
  log("= testEmergencyLabel = " + number + " should be " +
      (emergency ? "emergency" : "normal") + " call");

  let outCall;

  return gDial(number)
    .then(call => outCall = call)
    .then(() => is(outCall.emergency, emergency, "check emergency"))
    .then(() => gRemoteAnswer(outCall))
    .then(() => is(outCall.emergency, emergency, "check emergency"))
    .then(() => gRemoteHangUp(outCall));
}

startTest(function() {
  let origEccList;
  let eccList;

  getEccListProperty()
    .then(list => {
      origEccList = eccList = list;
    })
    .then(() => testEmergencyLabel("112", eccList))
    .then(() => testEmergencyLabel("911", eccList))
    .then(() => testEmergencyLabel("0912345678", eccList))
    .then(() => testEmergencyLabel("777", eccList))
    .then(() => {
      eccList = "777,119";
      return setEccListProperty(eccList);
    })
    .then(() => testEmergencyLabel("777", eccList))
    .then(() => testEmergencyLabel("119", eccList))
    .then(() => testEmergencyLabel("112", eccList))
    .then(() => setEccListProperty(origEccList))
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
