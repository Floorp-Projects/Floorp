/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

SpecialPowers.addPermission("sms", true, document);
SpecialPowers.setBoolPref("dom.sms.enabled", true);

let sms = window.navigator.mozSms;

// Specified here: https://developer.mozilla.org/en-US/docs/DOM/SmsManager
let maxCharsPerSms = 160;

function verifyInitialState() {
  log("Verifying initial state.");
  ok(sms, "mozSms");
  // Ensure test is starting clean with no existing SMS messages
  startTest();
}

function startTest() {
  // Build test data strings
  let testData = ["", "a",
                  buildString("b", maxCharsPerSms / 2),
                  buildString("c", maxCharsPerSms - 1),
                  buildString("d", maxCharsPerSms),
                  buildString("e", maxCharsPerSms + 1),
                  buildString("f", maxCharsPerSms * 1.5),
                  buildString("g", maxCharsPerSms * 10.5)];

  // Test with each data string
  testData.forEach(function(text){ testGetNumberOfMsgs(text); });

  // We're done
  cleanUp();
}

function buildString(char, numChars) {
  // Build string that contains the specified character repeated x times
  let string = new Array(numChars + 1).join(char);
  return string;
}

function testGetNumberOfMsgs(text) {
  // Test that getNumberOfMessagesForText returns expected value for given txt
  log("getNumberOfMessagesForText length " + text.length + ".");

  if (text.length) {
    if (text.length > maxCharsPerSms) {
      expNumSms = Math.ceil(text.length / maxCharsPerSms);
    } else {
      expNumSms = 1;
    }
  } else {
    expNumSms = 0;
  }

  numMultiPartSms = sms.getNumberOfMessagesForText(text);

  if (numMultiPartSms == expNumSms) {
    log("Returned " + expNumSms + " as expected.");
    ok(true, "getNumberOfMessagesForText returned expected value");
  } else {
    log("Returned " + numMultiPartSms + " but expected " + expNumSms + ".");
    ok(false, "getNumberOfMessagesForText returned unexpected value");
  }
}

function cleanUp() {
  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  finish();
}

// Start the test
verifyInitialState();
