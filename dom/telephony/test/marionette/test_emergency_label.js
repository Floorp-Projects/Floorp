/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const DEFAULT_ECC_LIST = "112,911";

function setEccListProperty(list) {
  log("Set property ril.ecclist: " + list);

  let deferred = Promise.defer();
  try {
    emulator.runShellCmd(["setprop","ril.ecclist", list]).then(function() {
      deferred.resolve(list);
    });
  } catch (e) {
    deferred.reject(e);
  }
  return deferred.promise;
}

function getEccListProperty() {
  log("Get property ril.ecclist.");

  let deferred = Promise.defer();
  try {
    emulator.runShellCmd(["getprop","ril.ecclist"]).then(function(aResult) {
      let list = !aResult.length ? "" : aResult[0];
      deferred.resolve(list);
    });
  } catch (e) {
    deferred.reject(e);
  }
  return deferred.promise;
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
    .then(call => { outCall = call; })
    .then(() => {
      is(outCall.emergency, emergency, "emergency result should be correct");
    })
    .then(() => gRemoteAnswer(outCall))
    .then(() => {
      is(outCall.emergency, emergency, "emergency result should be correct");
    })
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
    .then(null, error => {
      ok(false, 'promise rejects during test: ' + error);
    })
    .then(finish);
});
