/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'mmdb_head.js';

const DBNAME = "test_mmdb_full_storage:" + newUUID();

function newSavableMessage(aSender, aReceiver) {
  return {
    type: "sms",
    sender: aSender ? aSender : "+0987654321",
    receiver: aReceiver? aReceiver : "+1234567890",
    body: "quick fox jump over the lazy dog",
    deliveryStatusRequested: false,
    messageClass: "normal",
    timestamp: Date.now(),
    iccId: "1029384756"
  };
}

function setStorageFull(aFull) {
  SpecialPowers.notifyObserversInParentProcess(null,
                                               "disk-space-watcher",
                                               aFull ? "full" : "free");
}

function isFileNoDeviceSpaceError(aErrorResult) {
  is(aErrorResult, Cr.NS_ERROR_FILE_NO_DEVICE_SPACE, "Database error code");
}

function isCallbackStorageFullError(aErrorCode) {
  is(aErrorCode, Ci.nsIMobileMessageCallback.STORAGE_FULL_ERROR,
     "nsIMobileMessageCallback error code");
}

function testSaveSendingMessage(aMmdb) {
  log("testSaveSendingMessage()");

  setStorageFull(true);
  return saveSendingMessage(aMmdb, newSavableMessage())
    // Resolved/rejected results are both [<Cr.NS_ERROR_FOO>, <DOM message>],
    // and we need only the error code in both cases.
    .then((aValue) => aValue[0],
          (aValue) => aValue[0])
    .then(isFileNoDeviceSpaceError)
    .then(() => setStorageFull(false));
}

function testSaveReceivedMessage(aMmdb) {
  log("testSaveReceivedMessage()");

  setStorageFull(true);
  return saveReceivedMessage(aMmdb, newSavableMessage())
    // Resolved/rejected results are both [<Cr.NS_ERROR_FOO>, <DOM message>],
    // and we need only the error code in both cases.
    .then((aValue) => aValue[0],
          (aValue) => aValue[0])
    .then(isFileNoDeviceSpaceError)
    .then(() => setStorageFull(false));
}

function testGetMessageRecordById(aMmdb) {
  log("testGetMessageRecordById()");

  setStorageFull(false);
  return saveReceivedMessage(aMmdb, newSavableMessage())
    // Resolved result is [<Cr.NS_ERROR_FOO>, <DOM message>],
    .then(function(aValue) {
      let domMessage = aValue[1];

      setStorageFull(true);
      return getMessageRecordById(aMmdb, domMessage.id)
        .then(() => setStorageFull(false));
    });
}

function testMarkMessageRead(aMmdb) {
  log("testMarkMessageRead()");

  setStorageFull(false);
  return saveReceivedMessage(aMmdb, newSavableMessage())
    // Resolved/rejected results are both [<Cr.NS_ERROR_FOO>, <DOM message>].
    .then(function(aValue) {
      let domMessage = aValue[1];

      setStorageFull(true);
      return markMessageRead(aMmdb, domMessage.id, true)
        .then(null, (aValue) => aValue)
        .then(isCallbackStorageFullError)
        .then(() => setStorageFull(false));
    });
}

function testDeleteMessage(aMmdb) {
  log("testDeleteMessage()");

  // Note:
  // Due to thread-based design in MobileMessageDB and transaction-based design
  // in indexedDB, There are 2 restrictions to delete a message when storage full
  // in the following condition:
  // If the deletion won't remove all the messages in a thead and
  // 1. the last message of the thread or
  // 2. an unread message
  // are to be deleted.
  //
  // This will trigger an update of the thread's subject, message body,
  // unread count, etc but update operation is restricted when storage is full.

  let testAddress = "1111111111";
  let savedMsgIds = [];
  let promises = [];
  let numOfTestMessages = 5;

  setStorageFull(false);
  // Save several unread messages to the same thread then delete them.
  for (let i = 0; i < numOfTestMessages; i++) {
    promises.push(saveReceivedMessage(aMmdb, newSavableMessage(testAddress))
      .then((aValue) => { savedMsgIds.push(aValue[1].id); }));
  }

  return Promise.all(promises)
    .then(() => setStorageFull(true))

    // Failure is expected when deleting the last one.
    .then(() => deleteMessage(aMmdb, [savedMsgIds[numOfTestMessages - 1]], 1))
    .then(null, (aValue) => aValue)
    .then(isCallbackStorageFullError)

    // Failure is expected when deleting an unread message.
    .then(() => deleteMessage(aMmdb, [savedMsgIds[0]], 1))
    .then(null, (aValue) => aValue)
    .then(isCallbackStorageFullError)

    // Delete all messages in the thread.
    .then(() => deleteMessage(aMmdb, savedMsgIds, savedMsgIds.length))

    .then(() => setStorageFull(false));
}

function testSaveSmsSegment(aMmdb) {
  log("testSaveSmsSegment()");

  let smsSegment = {
    sender: "+0987654321",
    encoding: 0x00, // GSM 7bit alphabet
    iccId: "1029384756",
    segmentRef: 0,
    segmentSeq: 1,
    segmentMaxSeq: 3,
    body: "quick fox jump over the lazy dog"
  }

  setStorageFull(true);
  return saveSmsSegment(aMmdb, smsSegment)
    // Resolved/rejected results are both [<Cr.NS_ERROR_FOO>, <completeMessage>],
    // and we need only the error code in both cases.
    .then((aValue) => aValue[0],
          (aValue) => aValue[0])
    .then(isFileNoDeviceSpaceError)
    .then(() => setStorageFull(false));
}

function testCreateMessageCursor(aMmdb) {
  log("testCreateMessageCursor()");

  setStorageFull(true);
  return createMessageCursor(aMmdb, {}, false)
    .then(() => setStorageFull(false));
}

function testCreateThreadCursor(aMmdb) {
  log("testCreateThreadCursor()");

  setStorageFull(true);
  return createThreadCursor(aMmdb)
    .then(() => setStorageFull(false));
}

startTestBase(function testCaseMain() {

  let mmdb = newMobileMessageDB();
  return initMobileMessageDB(mmdb, DBNAME, 0)

    .then(() => testSaveSendingMessage(mmdb))
    .then(() => testSaveReceivedMessage(mmdb))
    .then(() => testGetMessageRecordById(mmdb))
    .then(() => testMarkMessageRead(mmdb))
    .then(() => testDeleteMessage(mmdb))
    .then(() => testSaveSmsSegment(mmdb))
    .then(() => testCreateMessageCursor(mmdb))
    .then(() => testCreateThreadCursor(mmdb))

    .then(() => closeMobileMessageDB(mmdb));
});
