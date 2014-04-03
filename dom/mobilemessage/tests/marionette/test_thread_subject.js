/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const PHONE_NUMBER = "+1234567890";

// Have a long long subject causes the send fails, so we don't need
// networking here.
const MMS_MAX_LENGTH_SUBJECT = 40;
function genMmsSubject(sep) {
  return "Hello " + (new Array(MMS_MAX_LENGTH_SUBJECT).join(sep)) + " World!";
}

function testSms(aProgressStr, aText) {
  log("Testing thread subject: " + aProgressStr);

  return sendSmsWithSuccess(PHONE_NUMBER, aText)
    .then(function(message) {
      log("  SMS sent, retrieving thread of id " + message.threadId);
      return getThreadById(message.threadId);
    })
    .then(function(thread) {
      log("  Got thread.lastMessageSubject = '" + thread.lastMessageSubject + "'");
      is(thread.lastMessageSubject, "", "thread.lastMessageSubject");
    });
}

function testMms(aProgressStr, aSubject) {
  log("Testing thread subject: " + aProgressStr);

  let mmsParameters = {
    receivers: [PHONE_NUMBER],
    subject: aSubject,
    attachments: [],
  };

  // We use a long long message subject so it will always fail.
  return sendMmsWithFailure(mmsParameters)
    .then(function(result) {
      log("  MMS sent, retrieving thread of id " + result.message.threadId);
      return getThreadById(result.message.threadId);
    })
    .then(function(thread) {
      log("  Got thread.lastMessageSubject = '" + thread.lastMessageSubject + "'");
      is(thread.lastMessageSubject, aSubject, "thread.lastMessageSubject");
    });
}

startTestCommon(function testCaseMain() {
  return testSms("SMS", "text")
    .then(testMms.bind(null, "SMS..MMS", genMmsSubject(" ")))
    .then(testSms.bind(null, "SMS..MMS..SMS", "text"))
    .then(deleteAllMessages)
    .then(testMms.bind(null, "MMS", genMmsSubject(" ")))
    .then(testSms.bind(null, "MMS..SMS", "text"))
    .then(testMms.bind(null, "MMS..SMS..MMS", genMmsSubject(" ")))
    .then(deleteAllMessages)
    .then(testSms.bind(null, "SMS", "1"))
    .then(testSms.bind(null, "SMS..SMS", "2"))
    .then(testSms.bind(null, "SMS..SMS..SMS", "3"))
    .then(deleteAllMessages)
    .then(testMms.bind(null, "MMS", genMmsSubject("a")))
    .then(testMms.bind(null, "MMS..MMS", genMmsSubject("b")))
    .then(testMms.bind(null, "MMS..MMS..MMS", genMmsSubject("c")));
});
