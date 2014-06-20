/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const ALLOWED = 0;
const RESTRICTED = 1;
const UNKNOWN = 2;
const PAYPHONE =3;

function getPresentationStr(presentation) {
  let str;
  switch (presentation) {
    case ALLOWED:
      str = "allowed";
      break;
    case RESTRICTED:
      str = "restricted";
      break;
    case UNKNOWN:
      str = "unknown";
      break;
    case PAYPHONE:
      str = "payphone";
      break;
  }
  return str;
}

function getNamePresentation(numberPresentation, namePresentation) {
  // See TS 23.096 Figure 3a and Annex A Note 1.
  if (!numberPresentation) {
    if (namePresentation == undefined) {
      namePresentation = ALLOWED;
    }
  } else {
    namePresentation = ALLOWED;
  }
  return getPresentationStr(namePresentation);
}

function test(number, numberPresentation, name, namePresentation) {
  return gRemoteDial(number, numberPresentation, name, namePresentation)
    .then(call => {
      is(call.id.numberPresentation, getPresentationStr(numberPresentation),
         "check numberPresentation");
      is(call.id.namePresentation,
         getNamePresentation(numberPresentation, namePresentation),
         "check namePresentation");
      return call;
    })
    .then(gHangUp);
}

startTest(function() {
  let inNumber = "5555550201";
  Promise.resolve()
    .then(() => test(inNumber, ALLOWED))
    .then(() => test(inNumber, RESTRICTED))
    .then(() => test(inNumber, UNKNOWN))
    .then(() => test(inNumber, PAYPHONE))
    .then(() => test(inNumber, ALLOWED, "TestName"))
    .then(() => test(inNumber, ALLOWED, "TestName", ALLOWED))
    .then(() => test(inNumber, ALLOWED, "TestName", RESTRICTED))
    .then(() => test(inNumber, ALLOWED, "TestName", UNKNOWN))
    .then(() => test(inNumber, RESTRICTED, "TestName", ALLOWED))
    .then(() => test(inNumber, RESTRICTED, "TestName", RESTRICTED))
    .then(() => test(inNumber, RESTRICTED, "TestName", UNKNOWN))
    .then(finish);
});
