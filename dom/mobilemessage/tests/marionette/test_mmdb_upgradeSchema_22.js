/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'mmdb_head.js';

Cu.import("resource://gre/modules/PhoneNumberUtils.jsm");

var RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

var MMS = {};
Cu.import("resource://gre/modules/MmsPduHelper.jsm", MMS);

const DBNAME = "test_mmdb_upgradeSchema_22:" + newUUID();

const MESSAGE_STORE_NAME = "sms";
const THREAD_STORE_NAME = "thread";
const PARTICIPANT_STORE_NAME = "participant";

const DEBUG = false;

const READ_WRITE = "readwrite";

const DELIVERY_SENDING = "sending";
const DELIVERY_SENT = "sent";
const DELIVERY_RECEIVED = "received";
const DELIVERY_NOT_DOWNLOADED = "not-downloaded";
const DELIVERY_ERROR = "error";

const DELIVERY_STATUS_NOT_APPLICABLE = "not-applicable";
const DELIVERY_STATUS_SUCCESS = "success";
const DELIVERY_STATUS_PENDING = "pending";
const DELIVERY_STATUS_ERROR = "error";

const MESSAGE_CLASS_NORMAL = "normal";

const FILTER_READ_UNREAD = 0;
const FILTER_READ_READ = 1;

const DISABLE_MMS_GROUPING_FOR_RECEIVING = true;

var LEGACY = {
  saveRecord: function(aMessageRecord, aAddresses, aCallback) {
    if (DEBUG) debug("Going to store " + JSON.stringify(aMessageRecord));

    let self = this;
    this.newTxn(READ_WRITE, function(error, txn, stores) {
      let notifyResult = function(aRv, aMessageRecord) {
        if (!aCallback) {
          return;
        }
        let domMessage =
          aMessageRecord && self.createDomMessageFromRecord(aMessageRecord);
        aCallback.notify(aRv, domMessage);
      };

      if (error) {
        notifyResult(error, null);
        return;
      }

      txn.oncomplete = function oncomplete(event) {
        if (aMessageRecord.id > self.lastMessageId) {
          self.lastMessageId = aMessageRecord.id;
        }
        notifyResult(Cr.NS_OK, aMessageRecord);
      };
      txn.onabort = function onabort(event) {
        // TODO bug 832140 check event.target.errorCode
        notifyResult(Cr.NS_ERROR_FAILURE, null);
      };

      let messageStore = stores[0];
      let participantStore = stores[1];
      let threadStore = stores[2];
      LEGACY.replaceShortMessageOnSave.call(self, txn, messageStore,
                                            participantStore, threadStore,
                                            aMessageRecord, aAddresses);
    }, [MESSAGE_STORE_NAME, PARTICIPANT_STORE_NAME, THREAD_STORE_NAME]);
  },

  replaceShortMessageOnSave: function(aTransaction, aMessageStore,
                                      aParticipantStore, aThreadStore,
                                      aMessageRecord, aAddresses) {
    let isReplaceTypePid = (aMessageRecord.pid) &&
                           ((aMessageRecord.pid >= RIL.PDU_PID_REPLACE_SHORT_MESSAGE_TYPE_1 &&
                             aMessageRecord.pid <= RIL.PDU_PID_REPLACE_SHORT_MESSAGE_TYPE_7) ||
                            aMessageRecord.pid == RIL.PDU_PID_RETURN_CALL_MESSAGE);

    if (aMessageRecord.type != "sms" ||
        aMessageRecord.delivery != DELIVERY_RECEIVED ||
        !isReplaceTypePid) {
      LEGACY.realSaveRecord.call(this, aTransaction, aMessageStore,
                                 aParticipantStore, aThreadStore,
                                 aMessageRecord, aAddresses);
      return;
    }

    // 3GPP TS 23.040 subclause 9.2.3.9 "TP-Protocol-Identifier (TP-PID)":
    //
    //   ... the MS shall check the originating address and replace any
    //   existing stored message having the same Protocol Identifier code
    //   and originating address with the new short message and other
    //   parameter values. If there is no message to be replaced, the MS
    //   shall store the message in the normal way. ... it is recommended
    //   that the SC address should not be checked by the MS."
    let self = this;
    this.findParticipantRecordByPlmnAddress(aParticipantStore,
                                            aMessageRecord.sender, false,
                                            function(participantRecord) {
      if (!participantRecord) {
        LEGACY.realSaveRecord.call(self, aTransaction, aMessageStore,
                                   aParticipantStore, aThreadStore,
                                   aMessageRecord, aAddresses);
        return;
      }

      let participantId = participantRecord.id;
      let range = IDBKeyRange.bound([participantId, 0], [participantId, ""]);
      let request = aMessageStore.index("participantIds").openCursor(range);
      request.onsuccess = function onsuccess(event) {
        let cursor = event.target.result;
        if (!cursor) {
          LEGACY.realSaveRecord.call(self, aTransaction, aMessageStore,
                                     aParticipantStore, aThreadStore,
                                     aMessageRecord, aAddresses);
          return;
        }

        // A message record with same participantId found.
        // Verify matching criteria.
        let foundMessageRecord = cursor.value;
        if (foundMessageRecord.type != "sms" ||
            foundMessageRecord.sender != aMessageRecord.sender ||
            foundMessageRecord.pid != aMessageRecord.pid) {
          cursor.continue();
          return;
        }

        // Match! Now replace that found message record with current one.
        aMessageRecord.id = foundMessageRecord.id;
        LEGACY.realSaveRecord.call(self, aTransaction, aMessageStore,
                                   aParticipantStore, aThreadStore,
                                   aMessageRecord, aAddresses);
      };
    });
  },

  realSaveRecord: function(aTransaction, aMessageStore, aParticipantStore,
                           aThreadStore, aMessageRecord, aAddresses) {
    let self = this;
    this.findThreadRecordByPlmnAddresses(aThreadStore, aParticipantStore,
                                         aAddresses, true,
                                         function(threadRecord, participantIds) {
      if (!participantIds) {
        aTransaction.abort();
        return;
      }

      let isOverriding = (aMessageRecord.id !== undefined);
      if (!isOverriding) {
        // |self.lastMessageId| is only updated in |txn.oncomplete|.
        aMessageRecord.id = self.lastMessageId + 1;
      }

      let timestamp = aMessageRecord.timestamp;
      let insertMessageRecord = function(threadId) {
        // Setup threadId & threadIdIndex.
        aMessageRecord.threadId = threadId;
        aMessageRecord.threadIdIndex = [threadId, timestamp];
        // Setup participantIdsIndex.
        aMessageRecord.participantIdsIndex = [];
        for (let id of participantIds) {
          aMessageRecord.participantIdsIndex.push([id, timestamp]);
        }

        if (!isOverriding) {
          // Really add to message store.
          aMessageStore.put(aMessageRecord);
          return;
        }

        // If we're going to override an old message, we need to update the
        // info of the original thread containing the overridden message.
        // To get the original thread ID and read status of the overridden
        // message record, we need to retrieve it before overriding it.
        aMessageStore.get(aMessageRecord.id).onsuccess = function(event) {
          let oldMessageRecord = event.target.result;
          aMessageStore.put(aMessageRecord);
          if (oldMessageRecord) {
            self.updateThreadByMessageChange(aMessageStore,
                                             aThreadStore,
                                             oldMessageRecord.threadId,
                                             [aMessageRecord.id],
                                             oldMessageRecord.read ? 0 : 1,
                                             null);
          }
        };
      };

      if (threadRecord) {
        let needsUpdate = false;

        if (threadRecord.lastTimestamp <= timestamp) {
          let lastMessageSubject;
          if (aMessageRecord.type == "mms") {
            lastMessageSubject = aMessageRecord.headers.subject;
          }
          threadRecord.lastMessageSubject = lastMessageSubject || null;
          threadRecord.lastTimestamp = timestamp;
          threadRecord.body = aMessageRecord.body;
          threadRecord.lastMessageId = aMessageRecord.id;
          threadRecord.lastMessageType = aMessageRecord.type;
          needsUpdate = true;
        }

        if (!aMessageRecord.read) {
          threadRecord.unreadCount++;
          needsUpdate = true;
        }

        if (needsUpdate) {
          aThreadStore.put(threadRecord);
        }

        insertMessageRecord(threadRecord.id);
        return;
      }

      let lastMessageSubject;
      if (aMessageRecord.type == "mms") {
        lastMessageSubject = aMessageRecord.headers.subject;
      }

      threadRecord = {
        participantIds: participantIds,
        participantAddresses: aAddresses,
        lastMessageId: aMessageRecord.id,
        lastTimestamp: timestamp,
        lastMessageSubject: lastMessageSubject || null,
        body: aMessageRecord.body,
        unreadCount: aMessageRecord.read ? 0 : 1,
        lastMessageType: aMessageRecord.type,
      };
      aThreadStore.add(threadRecord).onsuccess = function(event) {
        let threadId = event.target.result;
        insertMessageRecord(threadId);
      };
    });
  },

  fillReceivedMmsThreadParticipants: function(aMessage, threadParticipants) {
    let receivers = aMessage.receivers;
    // If we don't want to disable the MMS grouping for receiving, we need to
    // add the receivers (excluding the user's own number) to the participants
    // for creating the thread. Some cases might be investigated as below:
    //
    // 1. receivers.length == 0
    //    This usually happens when receiving an MMS notification indication
    //    which doesn't carry any receivers.
    // 2. receivers.length == 1
    //    If the receivers contain single phone number, we don't need to
    //    add it into participants because we know that number is our own.
    // 3. receivers.length >= 2
    //    If the receivers contain multiple phone numbers, we need to add all
    //    of them but not the user's own number into participants.
    if (DISABLE_MMS_GROUPING_FOR_RECEIVING || receivers.length < 2) {
      return;
    }
    let isSuccess = false;
    let slicedReceivers = receivers.slice();
    if (aMessage.msisdn) {
      let found = slicedReceivers.indexOf(aMessage.msisdn);
      if (found !== -1) {
        isSuccess = true;
        slicedReceivers.splice(found, 1);
      }
    }

    if (!isSuccess) {
      // For some SIMs we cannot retrieve the vaild MSISDN (i.e. the user's
      // own phone number), so we cannot correcly exclude the user's own
      // number from the receivers, thus wrongly building the thread index.
      if (DEBUG) debug("Error! Cannot strip out user's own phone number!");
    }

    threadParticipants = threadParticipants.concat(slicedReceivers);
  },

  saveReceivedMessage: function(aMessage, aCallback) {
    if ((aMessage.type != "sms" && aMessage.type != "mms") ||
        (aMessage.type == "sms" && (aMessage.messageClass == undefined ||
                                    aMessage.sender == undefined)) ||
        (aMessage.type == "mms" && (aMessage.delivery == undefined ||
                                    aMessage.deliveryStatus == undefined ||
                                    !Array.isArray(aMessage.receivers))) ||
        aMessage.timestamp == undefined) {
      if (aCallback) {
        aCallback.notify(Cr.NS_ERROR_FAILURE, null);
      }
      return;
    }

    let threadParticipants;
    if (aMessage.type == "mms") {
      if (aMessage.headers.from) {
        aMessage.sender = aMessage.headers.from.address;
      } else {
        aMessage.sender = "anonymous";
      }

      threadParticipants = [aMessage.sender];
      LEGACY.fillReceivedMmsThreadParticipants.call(this, aMessage,
                                                    threadParticipants);
    } else { // SMS
      threadParticipants = [aMessage.sender];
    }

    let timestamp = aMessage.timestamp;

    // Adding needed indexes and extra attributes for internal use.
    // threadIdIndex & participantIdsIndex are filled in saveRecord().
    aMessage.readIndex = [FILTER_READ_UNREAD, timestamp];
    aMessage.read = FILTER_READ_UNREAD;

    // If |sentTimestamp| is not specified, use 0 as default.
    if (aMessage.sentTimestamp == undefined) {
      aMessage.sentTimestamp = 0;
    }

    if (aMessage.type == "mms") {
      aMessage.transactionIdIndex = aMessage.headers["x-mms-transaction-id"];
      aMessage.isReadReportSent = false;

      // As a receiver, we don't need to care about the delivery status of
      // others, so we put a single element with self's phone number in the
      // |deliveryInfo| array.
      aMessage.deliveryInfo = [{
        receiver: aMessage.phoneNumber,
        deliveryStatus: aMessage.deliveryStatus,
        deliveryTimestamp: 0,
        readStatus: MMS.DOM_READ_STATUS_NOT_APPLICABLE,
        readTimestamp: 0,
      }];

      delete aMessage.deliveryStatus;
    }

    if (aMessage.type == "sms") {
      aMessage.delivery = DELIVERY_RECEIVED;
      aMessage.deliveryStatus = DELIVERY_STATUS_SUCCESS;
      aMessage.deliveryTimestamp = 0;

      if (aMessage.pid == undefined) {
        aMessage.pid = RIL.PDU_PID_DEFAULT;
      }
    }
    aMessage.deliveryIndex = [aMessage.delivery, timestamp];

    LEGACY.saveRecord.call(this, aMessage, threadParticipants, aCallback);
  },

  saveSendingMessage: function(aMessage, aCallback) {
    if ((aMessage.type != "sms" && aMessage.type != "mms") ||
        (aMessage.type == "sms" && aMessage.receiver == undefined) ||
        (aMessage.type == "mms" && !Array.isArray(aMessage.receivers)) ||
        aMessage.deliveryStatusRequested == undefined ||
        aMessage.timestamp == undefined) {
      if (aCallback) {
        aCallback.notify(Cr.NS_ERROR_FAILURE, null);
      }
      return;
    }

    // Set |aMessage.deliveryStatus|. Note that for MMS record
    // it must be an array of strings; For SMS, it's a string.
    let deliveryStatus = aMessage.deliveryStatusRequested
                       ? DELIVERY_STATUS_PENDING
                       : DELIVERY_STATUS_NOT_APPLICABLE;
    if (aMessage.type == "sms") {
      aMessage.deliveryStatus = deliveryStatus;
      // If |deliveryTimestamp| is not specified, use 0 as default.
      if (aMessage.deliveryTimestamp == undefined) {
        aMessage.deliveryTimestamp = 0;
      }
    } else if (aMessage.type == "mms") {
      let receivers = aMessage.receivers
      if (!Array.isArray(receivers)) {
        if (DEBUG) {
          debug("Need receivers for MMS. Fail to save the sending message.");
        }
        if (aCallback) {
          aCallback.notify(Cr.NS_ERROR_FAILURE, null);
        }
        return;
      }
      let readStatus = aMessage.headers["x-mms-read-report"]
                     ? MMS.DOM_READ_STATUS_PENDING
                     : MMS.DOM_READ_STATUS_NOT_APPLICABLE;
      aMessage.deliveryInfo = [];
      for (let i = 0; i < receivers.length; i++) {
        aMessage.deliveryInfo.push({
          receiver: receivers[i],
          deliveryStatus: deliveryStatus,
          deliveryTimestamp: 0,
          readStatus: readStatus,
          readTimestamp: 0,
        });
      }
    }

    let timestamp = aMessage.timestamp;

    // Adding needed indexes and extra attributes for internal use.
    // threadIdIndex & participantIdsIndex are filled in saveRecord().
    aMessage.deliveryIndex = [DELIVERY_SENDING, timestamp];
    aMessage.readIndex = [FILTER_READ_READ, timestamp];
    aMessage.delivery = DELIVERY_SENDING;
    aMessage.messageClass = MESSAGE_CLASS_NORMAL;
    aMessage.read = FILTER_READ_READ;

    // |sentTimestamp| is not available when the message is still sedning.
    aMessage.sentTimestamp = 0;

    let addresses;
    if (aMessage.type == "sms") {
      addresses = [aMessage.receiver];
    } else if (aMessage.type == "mms") {
      addresses = aMessage.receivers;
    }
    LEGACY.saveRecord.call(this, aMessage, addresses, aCallback);
  },
};

function callMmdbMethodLegacy(aMmdb, aMethodName) {
  let deferred = Promise.defer();

  let args = Array.slice(arguments, 2);
  args.push({
    notify: function(aRv) {
      if (!Components.isSuccessCode(aRv)) {
        ok(true, aMethodName + " returns a unsuccessful code: " + aRv);
        deferred.reject(Array.slice(arguments));
      } else {
        ok(true, aMethodName + " returns a successful code: " + aRv);
        deferred.resolve(Array.slice(arguments));
      }
    }
  });
  LEGACY[aMethodName].apply(aMmdb, args);

  return deferred.promise;
}

function saveSendingMessageLegacy(aMmdb, aMessage) {
  return callMmdbMethodLegacy(aMmdb, "saveSendingMessage", aMessage);
}

function saveReceivedMessageLegacy(aMmdb, aMessage) {
  return callMmdbMethodLegacy(aMmdb, "saveReceivedMessage", aMessage);
}

// Have a long long subject causes the send fails, so we don't need
// networking here.
const MMS_MAX_LENGTH_SUBJECT = 40;
function genMmsSubject(sep) {
  return "Hello " + (new Array(MMS_MAX_LENGTH_SUBJECT).join(sep)) + " World!";
}

function generateMms(aSender, aReceivers, aDelivery) {
  let message = {
    headers: {},
    type: "mms",
    timestamp: Date.now(),
    receivers: aReceivers,
    subject: genMmsSubject(' '),
    attachments: [],
  };

  message.headers.subject = message.subject;
  message.headers.to = [];
  for (let i = 0; i < aReceivers.length; i++) {
    let receiver = aReceivers[i];
    let entry = { type: MMS.Address.resolveType(receiver) };
    if (entry.type == "PLMN") {
      entry.address = PhoneNumberUtils.normalize(receiver, false);
    } else {
      entry.address = receiver;
    }
    ok(true, "MMS to address '" + receiver +"' resolved as type " + entry.type);
    message.headers.to.push(entry);
  }
  if (aSender) {
    message.headers.from = {
      address: aSender,
      type: MMS.Address.resolveType(aSender)
    };
    ok(true, "MMS from address '" + aSender +"' resolved as type " +
       message.headers.from.type);
  }

  if ((aDelivery === DELIVERY_RECEIVED) ||
      (aDelivery === DELIVERY_NOT_DOWNLOADED)) {
    message.delivery = aDelivery;
    message.deliveryStatus = DELIVERY_STATUS_SUCCESS;
  } else {
    message.deliveryStatusRequested = false;
  }

  return message;
}

function generateSms(aSender, aReceiver, aDelivery) {
  let message = {
    type: "sms",
    sender: aSender,
    timestamp: Date.now(),
    receiver: aReceiver,
    body: "The snow grows white on the mountain tonight.",
  };

  if (aDelivery === DELIVERY_RECEIVED) {
    message.messageClass = MESSAGE_CLASS_NORMAL;
  } else {
    message.deliveryStatusRequested = false;
  }

  return message;
}

function matchArray(lhs, rhs) {
  if (rhs.length != lhs.length) {
    return false;
  }

  for (let k = 0; k < lhs.length; k++) {
    if (lhs[k] != rhs[k]) {
      return false;
    }
  }

  return true;
}

const TEST_ADDRESSES = [
  "+15525225554",      // MMS, TYPE=PLMN
  "5525225554",        // MMS, TYPE=PLMN
  "jkalbcjklg",        // MMS, TYPE=PLMN, because of PhoneNumberNormalizer
  "jk.alb.cjk.lg",     // MMS, TYPE=PLMN, because of PhoneNumberNormalizer
  "j:k:a:l:b:c:jk:lg", // MMS, TYPE=PLMN, because of PhoneNumberNormalizer
  "55.252.255.54",     // MMS, TYPE=IPv4
  "5:5:2:5:2:2:55:54", // MMS, TYPE=IPv6
  "jk@alb.cjk.lg",     // MMS, TYPE=email
  "___"                // MMS, TYPE=Others
];

function populateDatabase(aMmdb) {
  log("Populating database:");

  let promise = Promise.resolve()

    // We're generating other messages that would be identified as the same
    // participant with "+15525225554".
    .then(() => saveReceivedMessageLegacy(
      aMmdb, generateSms("+15525225554", null, DELIVERY_RECEIVED)))

    // SMS, national number.
    .then(() => saveReceivedMessageLegacy(
      aMmdb, generateSms("5525225554", null, DELIVERY_RECEIVED)));

  for (let i = 0; i < TEST_ADDRESSES.length; i++) {
    let address = TEST_ADDRESSES[i];
    promise = promise.then(() => saveReceivedMessageLegacy(
      aMmdb, generateMms(address, ["a"], DELIVERY_RECEIVED)));
  }

  // Permutation of TEST_ADDRESSES.
  for (let i = 0; i < TEST_ADDRESSES.length; i++) {
    for (let j = i + 1; j < TEST_ADDRESSES.length; j++) {
      let addr_i = TEST_ADDRESSES[i], addr_j = TEST_ADDRESSES[j];
      promise = promise.then(() => saveSendingMessageLegacy(
        aMmdb, generateMms(null, [addr_i, addr_j], DELIVERY_SENDING)));
    }
  }

  // At this time, we have 3 threads, whose |participants| are:
  // ["+15525225554"], ["___"], and ["+15525225554", "___"].  The number of each
  // thread are [ (2 + 8 + 8 * 7 / 2), 1, 8 ] = [ 38, 1, 8 ].

  return promise;
}

function doVerifyDatabase(aMmdb, aExpected) {
  // 1) retrieve all threads.
  return createThreadCursor(aMmdb)
    .then(function(aValues) {
      let [errorCode, domThreads] = aValues;
      is(errorCode, 0, "errorCode");
      is(domThreads.length, aExpected.length, "domThreads.length");

      let numMessagesInThread = [];
      let totalMessages = 0;

      for (let i = 0; i < domThreads.length; i++) {
        let domThread = domThreads[i];
        log("  thread<" + domThread.id + "> : " + domThread.participants);

        let index = (function() {
          let rhs = domThread.participants;
          for (let j = 0; j < aExpected.length; j++) {
            let lhs = aExpected[j].participants;
            if (matchArray(lhs, rhs)) {
              return j;
            }
          }
        })();
        // 2) make sure all retrieved threads are in expected array.
        ok(index >= 0, "validity of domThread.participants");

        // 3) fill out numMessagesInThread, which is a <thread id> =>
        // <num messages> map.
        numMessagesInThread[domThread.id] = aExpected[index].messages;
        totalMessages += aExpected[index].messages

        aExpected.splice(index, 1);
      }

      // 4) make sure no thread is missing by checking |aExpected.length == 0|.
      is(aExpected.length, 0, "remaining unmatched threads");

      // 5) retrieve all messages.
      return createMessageCursor(aMmdb)
        .then(function(aValues) {
          let [errorCode, domMessages] = aValues;
          is(errorCode, 0, "errorCode");
          // 6) check total number of messages.
          is(domMessages.length, totalMessages, "domMessages.length");

          for (let i = 0; i < domMessages.length; i++) {
            let domMessage = domMessages[i];
            // 7) make sure message thread id is valid by checking
            //    |numMessagesInThread[domMessage.threadId] != null|.
            ok(numMessagesInThread[domMessage.threadId] != null,
               "domMessage.threadId");

            // 8) for each message, reduce
            // |numMessagesInThread[domMessage.threadId]| by 1.
            --numMessagesInThread[domMessage.threadId];
            ok(true, "numMessagesInThread[" + domMessage.threadId + "] = " +
                     numMessagesInThread[domMessage.threadId]);
          }

          // 9) check if |numMessagesInThread| is now an array of all zeros.
          for (let i = 0; i < domThreads.length; i++) {
            let domThread = domThreads[i];
            is(numMessagesInThread[domThread.id], 0,
               "numMessagesInThread[" + domThread.id + "]");
          }
        });
    });
}

function verifyDatabaseBeforeUpgrade(aMmdb) {
  log("Before updateSchema22:");
  return doVerifyDatabase(aMmdb, [{
    participants: ["+15525225554"],
    messages: 38 // 2 + (9 - 1) + (9 - 1) * ( 9 - 1 - 1) / 2
  }, {
    participants: ["___"],
    messages: 1
  }, {
    participants: ["+15525225554", "___"],
    messages: 8
  }]);
}

function verifyDatabaseAfterUpgrade(aMmdb) {
  log("After updateSchema22:");
  return doVerifyDatabase(aMmdb, [{
    participants: ["+15525225554"],
    messages: 17 // 2 + 5 + 5 * (5 - 1) / 2
  }, {
    participants: ["55.252.255.54"],
    messages: 1
  }, {
    participants: ["5:5:2:5:2:2:55:54"],
    messages: 1
  }, {
    participants: ["jk@alb.cjk.lg"],
    messages: 1
  }, {
    participants: ["___"],
    messages: 1
  }, {
    participants: ["+15525225554", "55.252.255.54"],
    messages: 5
  }, {
    participants: ["+15525225554", "5:5:2:5:2:2:55:54"],
    messages: 5
  }, {
    participants: ["+15525225554", "jk@alb.cjk.lg"],
    messages: 5
  }, {
    participants: ["+15525225554", "___"],
    messages: 5
  }, {
    participants: ["55.252.255.54", "5:5:2:5:2:2:55:54"],
    messages: 1
  }, {
    participants: ["55.252.255.54", "jk@alb.cjk.lg"],
    messages: 1
  }, {
    participants: ["55.252.255.54", "___"],
    messages: 1
  }, {
    participants: ["5:5:2:5:2:2:55:54", "jk@alb.cjk.lg"],
    messages: 1
  }, {
    participants: ["5:5:2:5:2:2:55:54", "___"],
    messages: 1
  }, {
    participants: ["jk@alb.cjk.lg", "___"],
    messages: 1
  }]);
}

startTestBase(function testCaseMain() {
  let mmdb = newMobileMessageDB();
  return initMobileMessageDB(mmdb, DBNAME, 22)
    .then(() => populateDatabase(mmdb))
    .then(() => verifyDatabaseBeforeUpgrade(mmdb))
    .then(() => closeMobileMessageDB(mmdb))

    .then(() => initMobileMessageDB(mmdb, DBNAME, 23))
    .then(() => verifyDatabaseAfterUpgrade(mmdb))
    .then(() => closeMobileMessageDB(mmdb));
});
