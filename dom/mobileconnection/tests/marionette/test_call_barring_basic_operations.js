/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const AO = MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING;
const OI = MozMobileConnection.CALL_BARRING_PROGRAM_OUTGOING_INTERNATIONAL;
const OX = MozMobileConnection.CALL_BARRING_PROGRAM_OUTGOING_INTERNATIONAL_EXCEPT_HOME;
let outgoingPrograms = [null, AO, OI, OX];

const AI = MozMobileConnection.CALL_BARRING_PROGRAM_ALL_INCOMING;
const IR = MozMobileConnection.CALL_BARRING_PROGRAM_INCOMING_ROAMING;
let incomingPrograms = [null, AI, IR];

const SERVICE_CLASS_VOICE = MozMobileConnection.ICC_SERVICE_CLASS_VOICE;

function getProgram(aProgram, aEnabled) {
  if (aProgram === null) {
    return Promise.resolve();
  }

  return Promise.resolve()
    .then(() => getCallBarringOption({
      program: aProgram,
      serviceClass: SERVICE_CLASS_VOICE
    }))

    .then(result => {
      is(result.program, aProgram, "Program");
      is(result.enabled, aEnabled, "Enabled");
      is(result.serviceClass, SERVICE_CLASS_VOICE, "ServiceClass");
    });
}

function setProgram(aProgram, aEnabled) {
  if (aProgram === null) {
    return Promise.resolve();
  }

  return Promise.resolve()
    .then(() => setCallBarringOption({
      program: aProgram,
      serviceClass: SERVICE_CLASS_VOICE,
      enabled: aEnabled,
      password: "0000" // The dafault call barring password of the emulator
    }));
}

function setAndCheckProgram(aActiveProgram, aAllPrograms) {
  let promise = Promise.resolve();

  if (aActiveProgram !== null) {
    promise = promise.then(() => setProgram(aActiveProgram, true));
  } else {
    // Deactive all barring programs in |aAllPrograms|.
    promise = aAllPrograms.reduce((previousPromise, program) => {
      return previousPromise.then(() => setProgram(program, false));
    }, promise);
  }

  // Make sure |aActiveProgram| is the only active program in |aAllPrograms|.
  promise = aAllPrograms.reduce((previousPromise, program) => {
    return previousPromise.then(() => getProgram(program, program === aActiveProgram));
  }, promise);

  return promise;
}

function testSingleProgram(aOriginProgram, aTargetPrograms, aOriginPrograms) {
  let promise = setAndCheckProgram(aOriginProgram, aOriginPrograms);

  return aTargetPrograms.reduce((previousPromise, targetProgram) => {
    return previousPromise
      .then(() => log(aOriginProgram + " <-> " + targetProgram))
      .then(() => setAndCheckProgram(targetProgram, aOriginPrograms))
      .then(() => setAndCheckProgram(aOriginProgram, aOriginPrograms));
  }, promise);
}

function testAllPrograms(aPrograms) {
  let targetPrograms = aPrograms.slice();

  return aPrograms.reduce((previousPromise, program) => {
    return previousPromise
      .then(() => {
        targetPrograms.shift();
        return testSingleProgram(program, targetPrograms, aPrograms);
      });
  }, Promise.resolve());
}

// Start tests
startTestCommon(function() {
  return Promise.resolve()
    .then(() => setProgram(null, outgoingPrograms))
    .then(() => setProgram(null, incomingPrograms))

    .then(() => log("=== Test outgoing call barring programs ==="))
    .then(() => testAllPrograms(outgoingPrograms))
    .then(() => log("=== Test incoming call barring programs ==="))
    .then(() => testAllPrograms(incomingPrograms))

    .catch(aError => ok(false, "promise rejects during test: " + aError))
    .then(() => setProgram(null, outgoingPrograms))
    .then(() => setProgram(null, incomingPrograms));
});