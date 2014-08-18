/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const REMOTE_NATIONAL_NUMBER = "552522555";
const REMOTE_INTERNATIONAL_NUMBER = "+1" + REMOTE_NATIONAL_NUMBER;

const TEXT_1 = "Nice to meet you";
const TEXT_2 = "Nice to meet you, too";

// Have a long long subject causes the send fails, so we don't need
// networking here.
const MMS_MAX_LENGTH_SUBJECT = 40;
function genMmsSubject(sep) {
  return "Hello " + (new Array(MMS_MAX_LENGTH_SUBJECT).join(sep)) + " World!";
}

function genFailingMms(aReceivers) {
  return {
    receivers: aReceivers,
    subject: genMmsSubject(' '),
    attachments: [],
  };
}

function checkMessage(aNeedle, aValidNumbers) {
  log("  Verifying " + aNeedle);

  let filter = { numbers: [aNeedle] };
  return getMessages(filter)
    .then(function(messages) {
      // Check the messages are sent to/received from aValidNumbers.
      is(messages.length, aValidNumbers.length, "messages.length");

      for (let message of messages) {
        let number;
        if (message.type == "sms") {
          number = (message.delivery === "received") ? message.sender
                                                     : message.receiver;
        } else {
          number = message.receivers[0];
        }

        let index = aValidNumbers.indexOf(number);
        ok(index >= 0, "message.number");
        aValidNumbers.splice(index, 1); // Remove from aValidNumbers.
      }

      is(aValidNumbers.length, 0, "aValidNumbers.length");
    });
}

startTestCommon(function testCaseMain() {
  return Promise.resolve()

    // SMS, MO:international, MT:national
    .then(() => sendSmsWithSuccess(REMOTE_INTERNATIONAL_NUMBER + '1', TEXT_1))
    .then(() => sendTextSmsToEmulator(REMOTE_NATIONAL_NUMBER + '1', TEXT_2))
    // SMS, MO:national, MT:international
    .then(() => sendSmsWithSuccess(REMOTE_NATIONAL_NUMBER + '2', TEXT_1))
    .then(() => sendTextSmsToEmulator(REMOTE_INTERNATIONAL_NUMBER + '2', TEXT_2))
    // MMS, international
    .then(() => sendMmsWithFailure(genFailingMms([REMOTE_INTERNATIONAL_NUMBER + '3'])))
    // MMS, national
    .then(() => sendMmsWithFailure(genFailingMms([REMOTE_NATIONAL_NUMBER + '3'])))
    // SMS, MO:international, MT:national, ambiguous number.
    .then(() => sendSmsWithSuccess(REMOTE_INTERNATIONAL_NUMBER + '4', TEXT_1))
    .then(() => sendTextSmsToEmulator(REMOTE_NATIONAL_NUMBER + '4', TEXT_2))
    .then(() => sendMmsWithFailure(genFailingMms(["jkalbcjklg"])))
    .then(() => sendMmsWithFailure(genFailingMms(["jk.alb.cjk.lg"])))
    // MMS, email
    .then(() => sendMmsWithFailure(genFailingMms(["jk@alb.cjk.lg"])))
    // MMS, IPv4
    .then(() => sendMmsWithFailure(genFailingMms(["55.252.255.54"])))
    // MMS, IPv6
    .then(() => sendMmsWithFailure(genFailingMms(["5:5:2:5:2:2:55:54"])))

    .then(() => checkMessage(REMOTE_INTERNATIONAL_NUMBER + '1',
                             [ REMOTE_INTERNATIONAL_NUMBER + '1',
                               REMOTE_NATIONAL_NUMBER + '1' ]))
    .then(() => checkMessage(REMOTE_NATIONAL_NUMBER + '2',
                             [ REMOTE_NATIONAL_NUMBER + '2',
                               REMOTE_INTERNATIONAL_NUMBER + '2' ]))
    .then(() => checkMessage(REMOTE_INTERNATIONAL_NUMBER + '3',
                             [ REMOTE_INTERNATIONAL_NUMBER + '3',
                               REMOTE_NATIONAL_NUMBER + '3' ]))
    .then(() => checkMessage(REMOTE_NATIONAL_NUMBER + '3',
                             [ REMOTE_NATIONAL_NUMBER + '3',
                               REMOTE_INTERNATIONAL_NUMBER + '3' ]))
    .then(() => checkMessage(REMOTE_NATIONAL_NUMBER + '4',
                             [ REMOTE_NATIONAL_NUMBER + '4',
                               REMOTE_INTERNATIONAL_NUMBER + '4',
                               "jkalbcjklg",
                               "jk.alb.cjk.lg" ]))
    .then(() => checkMessage("jk@alb.cjk.lg", ["jk@alb.cjk.lg"]))
    .then(() => checkMessage("55.252.255.54", ["55.252.255.54"]))
    .then(() => checkMessage("5:5:2:5:2:2:55:54", ["5:5:2:5:2:2:55:54"]))
});
