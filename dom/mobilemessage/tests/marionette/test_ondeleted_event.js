/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const DESTINATING_ADDRESS_1 = "0987654321";
const DESTINATING_ADDRESS_2 = "1234567890";
const OUT_TEXT = "Outgoing SMS message!";

function verifyDeletedInfo(aDeletedInfo, aMsgIds, aThreadIds) {
    is(JSON.stringify(aDeletedInfo.deletedMessageIds),
       JSON.stringify(aMsgIds), 'Check Msg Id.');
    is(JSON.stringify(aDeletedInfo.deletedThreadIds),
       JSON.stringify(aThreadIds), 'Check Thread Id.');
}

function testDeletingMessagesInOneThreadOneByOne() {
  let sentMessages = [];

  return Promise.resolve()
  .then(() => sendSmsWithSuccess(DESTINATING_ADDRESS_1, OUT_TEXT))
  .then((aMessage) => { sentMessages.push(aMessage); })
  .then(() => sendSmsWithSuccess(DESTINATING_ADDRESS_1, OUT_TEXT))
  .then((aMessage) => { sentMessages.push(aMessage); })
  .then(() => deleteMessagesById([sentMessages[0].id]))
  .then((aResult) => {
    verifyDeletedInfo(aResult.deletedInfo,
                      [sentMessages[0].id],
                      null);
  })
  .then(() => deleteMessagesById([sentMessages[1].id]))
  .then((aResult) => {
    verifyDeletedInfo(aResult.deletedInfo,
                      [sentMessages[1].id],
                      [sentMessages[1].threadId]);
  });
}

function testDeletingMessagesInTwoThreadsAtOnce() {
  let sentMessages = [];
  return Promise.resolve()
  .then(() => sendSmsWithSuccess(DESTINATING_ADDRESS_1, OUT_TEXT))
  .then((aMessage) => { sentMessages.push(aMessage); })
  .then(() => sendSmsWithSuccess(DESTINATING_ADDRESS_2, OUT_TEXT))
  .then((aMessage) => { sentMessages.push(aMessage); })
  .then(() =>
    deleteMessagesById(sentMessages.map((aMsg) => { return aMsg.id; })))
  .then((aResult) =>
    verifyDeletedInfo(aResult.deletedInfo,
                      sentMessages.map((aMsg) => { return aMsg.id; }),
                      sentMessages.map((aMsg) => { return aMsg.threadId; })));
}

startTestCommon(function testCaseMain() {
  return testDeletingMessagesInOneThreadOneByOne()
    .then(() => testDeletingMessagesInTwoThreadsAtOnce());
});
