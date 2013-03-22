/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PhoneNumberUtils.jsm");

const RIL_MOBILEMESSAGEDATABASESERVICE_CONTRACTID = "@mozilla.org/mobilemessage/rilmobilemessagedatabaseservice;1";
const RIL_MOBILEMESSAGEDATABASESERVICE_CID = Components.ID("{29785f90-6b5b-11e2-9201-3b280170b2ec}");

const DEBUG = false;
const DB_NAME = "sms";
const DB_VERSION = 8;
const MESSAGE_STORE_NAME = "sms";
const THREAD_STORE_NAME = "thread";
const PARTICIPANT_STORE_NAME = "participant";
const MOST_RECENT_STORE_NAME = "most-recent";

const DELIVERY_SENDING = "sending";
const DELIVERY_SENT = "sent";
const DELIVERY_RECEIVED = "received";

const DELIVERY_STATUS_NOT_APPLICABLE = "not-applicable";
const DELIVERY_STATUS_SUCCESS = "success";
const DELIVERY_STATUS_PENDING = "pending";
const DELIVERY_STATUS_ERROR = "error";

const MESSAGE_CLASS_NORMAL = "normal";

const FILTER_TIMESTAMP = "timestamp";
const FILTER_NUMBERS = "numbers";
const FILTER_DELIVERY = "delivery";
const FILTER_READ = "read";

// We can´t create an IDBKeyCursor with a boolean, so we need to use numbers
// instead.
const FILTER_READ_UNREAD = 0;
const FILTER_READ_READ = 1;

const READ_ONLY = "readonly";
const READ_WRITE = "readwrite";
const PREV = "prev";
const NEXT = "next";

XPCOMUtils.defineLazyServiceGetter(this, "gMobileMessageService",
                                   "@mozilla.org/mobilemessage/mobilemessageservice;1",
                                   "nsIMobileMessageService");

XPCOMUtils.defineLazyServiceGetter(this, "gIDBManager",
                                   "@mozilla.org/dom/indexeddb/manager;1",
                                   "nsIIndexedDatabaseManager");

const GLOBAL_SCOPE = this;

/**
 * MobileMessageDatabaseService
 */
function MobileMessageDatabaseService() {
  // Prime the directory service's cache to ensure that the ProfD entry exists
  // by the time IndexedDB queries for it off the main thread. (See bug 743635.)
  Services.dirsvc.get("ProfD", Ci.nsIFile);

  gIDBManager.initWindowless(GLOBAL_SCOPE);

  let that = this;
  this.newTxn(READ_ONLY, function(error, txn, messageStore){
    if (error) {
      return;
    }
    // In order to get the highest key value, we open a key cursor in reverse
    // order and get only the first pointed value.
    let request = messageStore.openCursor(null, PREV);
    request.onsuccess = function onsuccess(event) {
      let cursor = event.target.result;
      if (!cursor) {
        if (DEBUG) {
          debug("Could not get the last key from mobile message database. " +
                "Probably empty database");
        }
        return;
      }
      that.lastMessageId = cursor.key || 0;
      if (DEBUG) debug("Last assigned message ID was " + that.lastMessageId);
    };
    request.onerror = function onerror(event) {
      if (DEBUG) {
        debug("Could not get the last key from mobile message database " +
              event.target.errorCode);
      }
    };
  });

  this.messageLists = {};
}
MobileMessageDatabaseService.prototype = {

  classID: RIL_MOBILEMESSAGEDATABASESERVICE_CID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIRilMobileMessageDatabaseService,
                                         Ci.nsIMobileMessageDatabaseService,
                                         Ci.nsIObserver]),

  /**
   * Cache the DB here.
   */
  db: null,

  /**
   * This object keeps the message lists associated with each search. Each
   * message list is stored as an array of primary keys.
   */
  messageLists: null,

  lastMessageListId: 0,

  /**
   * Last sms/mms object store key value in the database.
   */
  lastMessageId: 0,

  /**
   * nsIObserver
   */
  observe: function observe() {},

  /**
   * Prepare the database. This may include opening the database and upgrading
   * it to the latest schema version.
   *
   * @param callback
   *        Function that takes an error and db argument. It is called when
   *        the database is ready to use or if an error occurs while preparing
   *        the database.
   *
   * @return (via callback) a database ready for use.
   */
  ensureDB: function ensureDB(callback) {
    if (this.db) {
      if (DEBUG) debug("ensureDB: already have a database, returning early.");
      callback(null, this.db);
      return;
    }

    let self = this;
    function gotDB(db) {
      self.db = db;
      callback(null, db);
    }

    let request = GLOBAL_SCOPE.indexedDB.open(DB_NAME, DB_VERSION);
    request.onsuccess = function (event) {
      if (DEBUG) debug("Opened database:", DB_NAME, DB_VERSION);
      gotDB(event.target.result);
    };
    request.onupgradeneeded = function (event) {
      if (DEBUG) {
        debug("Database needs upgrade:", DB_NAME,
              event.oldVersion, event.newVersion);
        debug("Correct new database version:", event.newVersion == DB_VERSION);
      }

      let db = event.target.result;

      let currentVersion = event.oldVersion;
      while (currentVersion != event.newVersion) {
        switch (currentVersion) {
          case 0:
            if (DEBUG) debug("New database");
            self.createSchema(db);
            break;
          case 1:
            if (DEBUG) debug("Upgrade to version 2. Including `read` index");
            self.upgradeSchema(event.target.transaction);
            break;
          case 2:
            if (DEBUG) debug("Upgrade to version 3. Fix existing entries.");
            self.upgradeSchema2(event.target.transaction);
            break;
          case 3:
            if (DEBUG) debug("Upgrade to version 4. Add quick threads view.");
            self.upgradeSchema3(db, event.target.transaction);
            break;
          case 4:
            if (DEBUG) debug("Upgrade to version 5. Populate quick threads view.");
            self.upgradeSchema4(event.target.transaction);
            break;
          case 5:
            if (DEBUG) debug("Upgrade to version 6. Use PhonenumberJS.");
            self.upgradeSchema5(event.target.transaction);
            break;
          case 6:
            if (DEBUG) debug("Upgrade to version 7. Use multiple entry indexes.");
            self.upgradeSchema6(event.target.transaction);
            break;
          case 7:
            if (DEBUG) debug("Upgrade to version 8. Add participant/thread stores.");
            self.upgradeSchema7(db, event.target.transaction);
            break;
          default:
            event.target.transaction.abort();
            callback("Old database version: " + event.oldVersion, null);
            break;
        }
        currentVersion++;
      }
    };
    request.onerror = function (event) {
      //TODO look at event.target.Code and change error constant accordingly
      callback("Error opening database!", null);
    };
    request.onblocked = function (event) {
      callback("Opening database request is blocked.", null);
    };
  },

  /**
   * Start a new transaction.
   *
   * @param txn_type
   *        Type of transaction (e.g. READ_WRITE)
   * @param callback
   *        Function to call when the transaction is available. It will
   *        be invoked with the transaction and opened object stores.
   * @param storeNames
   *        Names of the stores to open.
   */
  newTxn: function newTxn(txn_type, callback, storeNames) {
    if (!storeNames) {
      storeNames = [MESSAGE_STORE_NAME];
    }
    if (DEBUG) debug("Opening transaction for object stores: " + storeNames);
    this.ensureDB(function (error, db) {
      if (error) {
        if (DEBUG) debug("Could not open database: " + error);
        callback(error);
        return;
      }
      let txn = db.transaction(storeNames, txn_type);
      if (DEBUG) debug("Started transaction " + txn + " of type " + txn_type);
      if (DEBUG) {
        txn.oncomplete = function oncomplete(event) {
          debug("Transaction " + txn + " completed.");
        };
        txn.onerror = function onerror(event) {
          //TODO check event.target.errorCode and show an appropiate error
          //     message according to it.
          debug("Error occurred during transaction: " + event.target.errorCode);
        };
      }
      let stores;
      if (storeNames.length == 1) {
        if (DEBUG) debug("Retrieving object store " + storeNames[0]);
        stores = txn.objectStore(storeNames[0]);
      } else {
        stores = [];
        for each (let storeName in storeNames) {
          if (DEBUG) debug("Retrieving object store " + storeName);
          stores.push(txn.objectStore(storeName));
        }
      }
      callback(null, txn, stores);
    });
  },

  /**
   * Create the initial database schema.
   *
   * TODO need to worry about number normalization somewhere...
   * TODO full text search on body???
   */
  createSchema: function createSchema(db) {
    // This messageStore holds the main mobile message data.
    let messageStore = db.createObjectStore(MESSAGE_STORE_NAME, { keyPath: "id" });
    messageStore.createIndex("timestamp", "timestamp", { unique: false });
    if (DEBUG) debug("Created object stores and indexes");
  },

  /**
   * Upgrade to the corresponding database schema version.
   */
  upgradeSchema: function upgradeSchema(transaction) {
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);
    messageStore.createIndex("read", "read", { unique: false });
  },

  upgradeSchema2: function upgradeSchema2(transaction) {
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);
    messageStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (!cursor) {
        return;
      }

      let messageRecord = cursor.value;
      messageRecord.messageClass = MESSAGE_CLASS_NORMAL;
      messageRecord.deliveryStatus = DELIVERY_STATUS_NOT_APPLICABLE;
      cursor.update(messageRecord);
      cursor.continue();
    };
  },

  upgradeSchema3: function upgradeSchema3(db, transaction) {
    // Delete redundant "id" index.
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);
    if (messageStore.indexNames.contains("id")) {
      messageStore.deleteIndex("id");
    }

    /**
     * This mostRecentStore can be used to quickly construct a thread view of
     * the mobile message database. Each entry looks like this:
     *
     * { senderOrReceiver: <String> (primary key),
     *   id: <Number>,
     *   timestamp: <Date>,
     *   body: <String>,
     *   unreadCount: <Number> }
     *
     */
    let mostRecentStore = db.createObjectStore(MOST_RECENT_STORE_NAME,
                                               { keyPath: "senderOrReceiver" });
    mostRecentStore.createIndex("timestamp", "timestamp");
  },

  upgradeSchema4: function upgradeSchema4(transaction) {
    let threads = {};
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);
    let mostRecentStore = transaction.objectStore(MOST_RECENT_STORE_NAME);

    messageStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (!cursor) {
        for (let thread in threads) {
          mostRecentStore.put(threads[thread]);
        }
        return;
      }

      let messageRecord = cursor.value;
      let contact = messageRecord.sender || messageRecord.receiver;

      if (contact in threads) {
        let thread = threads[contact];
        if (!messageRecord.read) {
          thread.unreadCount++;
        }
        if (messageRecord.timestamp > thread.timestamp) {
          thread.id = messageRecord.id;
          thread.body = messageRecord.body;
          thread.timestamp = messageRecord.timestamp;
        }
      } else {
        threads[contact] = {
          senderOrReceiver: contact,
          id: messageRecord.id,
          timestamp: messageRecord.timestamp,
          body: messageRecord.body,
          unreadCount: messageRecord.read ? 0 : 1
        };
      }
      cursor.continue();
    };
  },

  upgradeSchema5: function upgradeSchema5(transaction) {
    // Don't perform any upgrade. See Bug 819560.
  },

  upgradeSchema6: function upgradeSchema6(transaction) {
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);

    // Delete "delivery" index.
    if (messageStore.indexNames.contains("delivery")) {
      messageStore.deleteIndex("delivery");
    }
    // Delete "sender" index.
    if (messageStore.indexNames.contains("sender")) {
      messageStore.deleteIndex("sender");
    }
    // Delete "receiver" index.
    if (messageStore.indexNames.contains("receiver")) {
      messageStore.deleteIndex("receiver");
    }
    // Delete "read" index.
    if (messageStore.indexNames.contains("read")) {
      messageStore.deleteIndex("read");
    }

    // Create new "delivery", "number" and "read" indexes.
    messageStore.createIndex("delivery", "deliveryIndex");
    messageStore.createIndex("number", "numberIndex", { multiEntry: true });
    messageStore.createIndex("read", "readIndex");

    // Populate new "deliverIndex", "numberIndex" and "readIndex" attributes.
    messageStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (!cursor) {
        return;
      }

      let messageRecord = cursor.value;
      let timestamp = messageRecord.timestamp;
      messageRecord.deliveryIndex = [messageRecord.delivery, timestamp];
      messageRecord.numberIndex = [
        [messageRecord.sender, timestamp],
        [messageRecord.receiver, timestamp]
      ];
      messageRecord.readIndex = [messageRecord.read, timestamp];
      cursor.update(messageRecord);
      cursor.continue();
    };
  },

  /**
   * Add participant/thread stores.
   *
   * The message store now saves original phone numbers/addresses input from
   * content to message records. No normalization is made.
   *
   * For filtering messages by phone numbers, it first looks up corresponding
   * participant IDs from participant table and fetch message records with
   * matching keys defined in per record "participantIds" field.
   *
   * For message threading, messages with the same participant ID array are put
   * in the same thread. So updating "unreadCount", "lastMessageId" and
   * "lastTimestamp" are through the "threadId" carried by per message record.
   * Fetching threads list is now simply walking through the thread sotre. The
   * "mostRecentStore" is dropped.
   */
  upgradeSchema7: function upgradeSchema7(db, transaction) {
    /**
     * This "participant" object store keeps mappings of multiple phone numbers
     * of the same recipient to an integer participant id. Each entry looks
     * like:
     *
     * { id: <Number> (primary key),
     *   addresses: <Array of strings> }
     */
    let participantStore = db.createObjectStore(PARTICIPANT_STORE_NAME,
                                                { keyPath: "id",
                                                  autoIncrement: true });
    participantStore.createIndex("addresses", "addresses", { multiEntry: true });

    /**
     * This "threads" object store keeps mappings from an integer thread id to
     * ids of the participants of that message thread. Each entry looks like:
     *
     * { id: <Number> (primary key),
     *   participantIds: <Array of participant IDs>,
     *   participantAddresses: <Array of the first addresses of the participants>,
     *   lastMessageId: <Number>,
     *   lastTimestamp: <Date>,
     *   subject: <String>,
     *   unreadCount: <Number> }
     *
     */
    let threadStore = db.createObjectStore(THREAD_STORE_NAME,
                                           { keyPath: "id",
                                             autoIncrement: true });
    threadStore.createIndex("participantIds", "participantIds");
    threadStore.createIndex("lastTimestamp", "lastTimestamp");

    /**
     * Replace "numberIndex" with "participantIdsIndex" and create an additional
     * "threadId". "numberIndex" will be removed later.
     */
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);
    messageStore.createIndex("threadId", "threadIdIndex");
    messageStore.createIndex("participantIds", "participantIdsIndex",
                             { multiEntry: true });

    // Now populate participantStore & threadStore.
    let mostRecentStore = transaction.objectStore(MOST_RECENT_STORE_NAME);
    let self = this;
    let mostRecentRequest = mostRecentStore.openCursor();
    mostRecentRequest.onsuccess = function(event) {
      let mostRecentCursor = event.target.result;
      if (!mostRecentCursor) {
        db.deleteObjectStore(MOST_RECENT_STORE_NAME);

        // No longer need the "number" index in messageStore, use
        // "participantIds" index instead.
        messageStore.deleteIndex("number");
        return;
      }

      let mostRecentRecord = mostRecentCursor.value;

      // Each entry in mostRecentStore is supposed to be a unique thread, so we
      // retrieve the records out and insert its "senderOrReceiver" column as a
      // new record in participantStore.
      let number = mostRecentRecord.senderOrReceiver;
      self.findParticipantRecordByAddress(participantStore, number, true,
                                          function (participantRecord) {
        // Also create a new record in threadStore.
        let threadRecord = {
          participantIds: [participantRecord.id],
          participantAddresses: [number],
          lastMessageId: mostRecentRecord.id,
          lastTimestamp: mostRecentRecord.timestamp,
          subject: mostRecentRecord.body,
          unreadCount: mostRecentRecord.unreadCount,
        };
        let addThreadRequest = threadStore.add(threadRecord);
        addThreadRequest.onsuccess = function (event) {
          threadRecord.id = event.target.result;

          let numberRange = IDBKeyRange.bound([number, 0], [number, ""]);
          let messageRequest = messageStore.index("number")
                                           .openCursor(numberRange, NEXT);
          messageRequest.onsuccess = function (event) {
            let messageCursor = event.target.result;
            if (!messageCursor) {
              // No more message records, check next most recent record.
              mostRecentCursor.continue();
              return;
            }

            let messageRecord = messageCursor.value;
            // Check whether the message really belongs to this thread.
            let matchSenderOrReceiver = false;
            if (messageRecord.delivery == DELIVERY_RECEIVED) {
              if (messageRecord.sender == number) {
                matchSenderOrReceiver = true;
              }
            } else if (messageRecord.receiver == number) {
              matchSenderOrReceiver = true;
            }
            if (!matchSenderOrReceiver) {
              // Check next message record.
              messageCursor.continue();
              return;
            }

            messageRecord.threadId = threadRecord.id;
            messageRecord.threadIdIndex = [threadRecord.id,
                                           messageRecord.timestamp];
            messageRecord.participantIdsIndex = [
              [participantRecord.id, messageRecord.timestamp]
            ];
            messageCursor.update(messageRecord);
            // Check next message record.
            messageCursor.continue();
          };
          messageRequest.onerror = function () {
            // Error in fetching message records, check next most recent record.
            mostRecentCursor.continue();
          };
        };
        addThreadRequest.onerror = function () {
          // Error in fetching message records, check next most recent record.
          mostRecentCursor.continue();
        };
      });
    };
  },

  createDomMessageFromRecord: function createDomMessageFromRecord(aMessageRecord) {
    if (DEBUG) {
      debug("createDomMessageFromRecord: " + JSON.stringify(aMessageRecord));
    }
    if (aMessageRecord.type == "sms") {
      return gMobileMessageService.createSmsMessage(aMessageRecord.id,
                                                    aMessageRecord.delivery,
                                                    aMessageRecord.deliveryStatus,
                                                    aMessageRecord.sender,
                                                    aMessageRecord.receiver,
                                                    aMessageRecord.body,
                                                    aMessageRecord.messageClass,
                                                    aMessageRecord.timestamp,
                                                    aMessageRecord.read);
    } else if (aMessageRecord.type == "mms") {
      let headers = aMessageRecord["headers"];
      if (DEBUG) {
        debug("MMS: headers: " + JSON.stringify(headers));
      }

      let subject = headers["subject"];
      if (subject == undefined) {
        subject = "";
      }

      let smil = "";
      let attachments = [];
      let parts = aMessageRecord.parts;
      if (parts) {
        for (let i = 0; i < parts.length; i++) {
          let part = parts[i];
          if (DEBUG) {
            debug("MMS: part[" + i + "]: " + JSON.stringify(part));
          }

          let partHeaders = part["headers"];
          let partContent = part["content"];
          // Don't need to make the SMIL part if it's present.
          if (partHeaders["content-type"]["media"] == "application/smil") {
            smil = partContent;
            continue;
          }
          attachments.push({
            "id": partHeaders["content-id"],
            "location": partHeaders["content-location"],
            "content": partContent
          });
        }
      }
      return gMobileMessageService.createMmsMessage(aMessageRecord.id,
                                                    aMessageRecord.delivery,
                                                    aMessageRecord.deliveryStatus,
                                                    aMessageRecord.sender,
                                                    aMessageRecord.receivers,
                                                    aMessageRecord.timestamp,
                                                    aMessageRecord.read,
                                                    subject,
                                                    smil,
                                                    attachments);
    }
  },

  /**
   * Queue up passed message id, reply if necessary. 'aMessageId' = 0 for no
   * more messages, negtive for errors and valid otherwise.
   */
  onNextMessageInListGot: function onNextMessageInListGot(
      aMessageStore, aMessageList, aMessageId) {

    if (DEBUG) {
      debug("onNextMessageInListGot - listId: "
            + aMessageList.listId + ", messageId: " + aMessageId);
    }
    if (aMessageId) {
      // Queue up any id but '0' and replies later accordingly.
      aMessageList.results.push(aMessageId);
    }
    if (aMessageId <= 0) {
      // No more processing on '0' or negative values passed.
      aMessageList.processing = false;
    }

    if (!aMessageList.requestWaiting) {
      if (DEBUG) debug("Cursor.continue() not called yet");
      return;
    }

    // We assume there is only one request waiting throughout the message list
    // retrieving process. So we don't bother continuing to process further
    // waiting requests here. This assumption comes from SmsCursor::Continue()
    // implementation.
    let request = aMessageList.requestWaiting;
    aMessageList.requestWaiting = null;

    if (!aMessageList.results.length) {
      // No fetched results yet.
      if (!aMessageList.processing) {
        if (DEBUG) debug("No messages matching the filter criteria");
        request.notifyNoMessageInList();
      }
      // No fetched results yet and still processing. Let's wait a bit more.
      return;
    }

    if (aMessageList.results[0] < 0) {
      // An previous error found. Keep the answer in results so that we can
      // reply INTERNAL_ERROR for further requests.
      if (DEBUG) debug("An previous error found");
      request.notifyReadMessageListFailed(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
      return;
    }

    let firstMessageId = aMessageList.results.shift();
    if (DEBUG) debug ("Fetching message " + firstMessageId);

    let getRequest = aMessageStore.get(firstMessageId);
    let self = this;
    getRequest.onsuccess = function onsuccess(event) {
      let messageRecord = event.target.result;
      let domMessage = self.createDomMessageFromRecord(messageRecord);
      if (aMessageList.listId >= 0) {
        if (DEBUG) {
          debug("notifyNextMessageInListGot - listId: "
                + aMessageList.listId + ", messageId: " + firstMessageId);
        }
        request.notifyNextMessageInListGot(domMessage);
      } else {
        self.lastMessageListId += 1;
        aMessageList.listId = self.lastMessageListId;
        self.messageLists[self.lastMessageListId] = aMessageList;
        if (DEBUG) {
          debug("notifyMessageListCreated - listId: "
                + aMessageList.listId + ", messageId: " + firstMessageId);
        }
        request.notifyMessageListCreated(aMessageList.listId, domMessage);
      }
    };
    getRequest.onerror = function onerror(event) {
      if (DEBUG) {
        debug("notifyReadMessageListFailed - listId: "
              + aMessageList.listId + ", messageId: " + firstMessageId);
      }
      request.notifyReadMessageListFailed(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
    };
  },

  /**
   * Queue up {aMessageId, aTimestamp} pairs, find out intersections and report
   * to onNextMessageInListGot. Return true if it is still possible to have
   * another match.
   */
  onNextMessageInMultiFiltersGot: function onNextMessageInMultiFiltersGot(
      aMessageStore, aMessageList, aContextIndex, aMessageId, aTimestamp) {

    if (DEBUG) {
      debug("onNextMessageInMultiFiltersGot: "
            + aContextIndex + ", " + aMessageId + ", " + aTimestamp);
    }
    let contexts = aMessageList.contexts;

    if (!aMessageId) {
      contexts[aContextIndex].processing = false;
      for (let i = 0; i < contexts.length; i++) {
        if (contexts[i].processing) {
          return false;
        }
      }

      this.onNextMessageInListGot(aMessageStore, aMessageList, 0);
      return false;
    }

    // Search id in other existing results. If no other results has it,
    // and A) the last timestamp is smaller-equal to current timestamp,
    // we wait for further results; either B) record timestamp is larger
    // then current timestamp or C) no more processing for a filter, then we
    // drop this id because there can't be a match anymore.
    for (let i = 0; i < contexts.length; i++) {
      if (i == aContextIndex) {
        continue;
      }

      let ctx = contexts[i];
      let results = ctx.results;
      let found = false;
      for (let j = 0; j < results.length; j++) {
        let result = results[j];
        if (result.id == aMessageId) {
          found = true;
          break;
        }
        if ((!aMessageList.reverse && (result.timestamp > aTimestamp)) ||
            (aMessageList.reverse && (result.timestamp < aTimestamp))) {
          // B) Cannot find a match anymore. Drop.
          return true;
        }
      }

      if (!found) {
        if (!ctx.processing) {
          // C) Cannot find a match anymore. Drop.
          if (results.length) {
            let lastResult = results[results.length - 1];
            if ((!aMessageList.reverse && (lastResult.timestamp >= aTimestamp)) ||
                (aMessageList.reverse && (lastResult.timestamp <= aTimestamp))) {
              // Still have a chance to get another match. Return true.
              return true;
            }
          }

          // Impossible to find another match because all results in ctx have
          // timestamps smaller than aTimestamp.
          return this.onNextMessageInMultiFiltersGot(aMessageStore, aMessageList,
                                                     aContextIndex, 0, 0);
        }

        // A) Pending.
        contexts[aContextIndex].results.push({
          id: aMessageId,
          timestamp: aTimestamp
        });
        return true;
      }
    }

    // Now id is found in all other results. Report it.
    this.onNextMessageInListGot(aMessageStore, aMessageList, aMessageId);
    return true;
  },

  onNextMessageInMultiNumbersGot: function onNextMessageInMultiNumbersGot(
      aMessageStore, aMessageList, aContextIndex,
      aQueueIndex, aMessageId, aTimestamp) {

    if (DEBUG) {
      debug("onNextMessageInMultiNumbersGot: "
            + aQueueIndex + ", " + aMessageId + ", " + aTimestamp);
    }
    let queues = aMessageList.numberQueues;
    let q = queues[aQueueIndex];
    if (aMessageId) {
      if (!aQueueIndex) {
        // Timestamp.
        q.results.push({
          id: aMessageId,
          timestamp: aTimestamp
        });
      } else {
        // Numbers.
        q.results.push(aMessageId);
      }
      return true;
    }

    q.processing -= 1;
    if (queues[0].processing || queues[1].processing) {
      // At least one queue is still processing, but we got here because
      // current cursor gives 0 as aMessageId meaning no more messages are
      // available. Return false here to stop further cursor.continue() calls.
      return false;
    }

    let tres = queues[0].results;
    let qres = queues[1].results;
    tres = tres.filter(function (element) {
      return qres.indexOf(element.id) != -1;
    });
    if (aContextIndex == null) {
      for (let i = 0; i < tres.length; i++) {
        this.onNextMessageInListGot(aMessageStore, aMessageList, tres[i].id);
      }
      this.onNextMessageInListGot(aMessageStore, aMessageList, 0);
    } else {
      for (let i = 0; i < tres.length; i++) {
        this.onNextMessageInMultiFiltersGot(aMessageStore, aMessageList,
                                            aContextIndex,
                                            tres[i].id, tres[i].timestamp);
      }
      this.onNextMessageInMultiFiltersGot(aMessageStore, aMessageList,
                                          aContextIndex, 0, 0);
    }
    return false;
  },

  findParticipantRecordByAddress: function findParticipantRecordByAddress(
      aParticipantStore, aAddress, aCreate, aCallback) {
    if (DEBUG) {
      debug("findParticipantRecordByAddress("
            + JSON.stringify(aAddress) + ", " + aCreate + ")");
    }

    // Two types of input number to match here, international(+886987654321),
    // and local(0987654321) types. The "nationalNumber" parsed from
    // phonenumberutils will be "987654321" in this case.

    let request = aParticipantStore.index("addresses").get(aAddress);
    request.onsuccess = (function (event) {
      let participantRecord = event.target.result;
      // 1) First try matching through "addresses" index of participant store.
      //    If we're lucky, return the fetched participant record.
      if (participantRecord) {
        if (DEBUG) {
          debug("findParticipantRecordByAddress: got "
                + JSON.stringify(participantRecord));
        }
        aCallback(participantRecord);
        return;
      }

      // Only parse aAddress if it's already an international number.
      let parsedAddress = PhoneNumberUtils.parseWithMCC(aAddress, null);
      // 2) Traverse throught all participants and check all alias addresses.
      aParticipantStore.openCursor().onsuccess = (function (event) {
        let cursor = event.target.result;
        if (!cursor) {
          // Have traversed whole object store but still in vain.
          if (!aCreate) {
            aCallback(null);
            return;
          }

          let participantRecord = { addresses: [aAddress] };
          let addRequest = aParticipantStore.add(participantRecord);
          addRequest.onsuccess = function (event) {
            participantRecord.id = event.target.result;
            if (DEBUG) {
              debug("findParticipantRecordByAddress: created "
                    + JSON.stringify(participantRecord));
            }
            aCallback(participantRecord);
          };
          return;
        }

        let participantRecord = cursor.value;
        for each (let storedAddress in participantRecord.addresses) {
          let match = false;
          if (parsedAddress) {
            // 2-1) If input number is an international one, then a potential
            //      participant must be stored as local type.  Then just check
            //      if stored number ends with the national number(987654321) of
            //      the input number.
            if (storedAddress.endsWith(parsedAddress.nationalNumber)) {
              match = true;
            }
          } else {
            // 2-2) Else if the stored number is an international one, then the
            //      input number must be local type.  Then just check whether
            //      does it ends with the national number of the stored number.
            let parsedStoredAddress =
              PhoneNumberUtils.parseWithMCC(storedAddress, null);
            if (parsedStoredAddress
                && aAddress.endsWith(parsedStoredAddress.nationalNumber)) {
              match = true;
            }
          }
          if (!match) {
            // 3) Else we fail to match current stored participant record.
            continue;
          }

          // Match!
          if (aCreate) {
            // In a READ-WRITE transaction, append one more possible address for
            // this participant record.
            participantRecord.addresses.push(aAddress);
            cursor.update(participantRecord);
          }
          if (DEBUG) {
            debug("findParticipantRecordByAddress: got "
                  + JSON.stringify(cursor.value));
          }
          aCallback(participantRecord);
          return;
        }

        // Check next participant record if available.
        cursor.continue();
      }).bind(this);
    }).bind(this);
  },

  findParticipantIdsByAddresses: function findParticipantIdsByAddresses(
      aParticipantStore, aAddresses, aCreate, aSkipNonexistent, aCallback) {
    if (DEBUG) {
      debug("findParticipantIdsByAddresses("
            + JSON.stringify(aAddresses) + ", "
            + aCreate + ", " + aSkipNonexistent + ")");
    }

    if (!aAddresses || !aAddresses.length) {
      if (DEBUG) debug("findParticipantIdsByAddresses: returning null");
      aCallback(null);
      return;
    }

    let self = this;
    (function findParticipantId(index, result) {
      if (index >= aAddresses.length) {
        // Sort numerically.
        result.sort(function (a, b) {
          return a - b;
        });
        if (DEBUG) debug("findParticipantIdsByAddresses: returning " + result);
        aCallback(result);
        return;
      }

      self.findParticipantRecordByAddress(aParticipantStore,
                                          aAddresses[index++], aCreate,
                                          function (participantRecord) {
        if (!participantRecord) {
          if (!aSkipNonexistent) {
            if (DEBUG) debug("findParticipantIdsByAddresses: returning null");
            aCallback(null);
            return;
          }
        } else if (result.indexOf(participantRecord.id) < 0) {
          result.push(participantRecord.id);
        }
        findParticipantId(index, result);
      });
    }) (0, []);
  },

  findThreadRecordByParticipants: function findThreadRecordByParticipants(
      aThreadStore, aParticipantStore, aAddresses,
      aCreateParticipants, aCallback) {
    if (DEBUG) {
      debug("findThreadRecordByParticipants(" + JSON.stringify(aAddresses)
            + ", " + aCreateParticipants + ")");
    }
    this.findParticipantIdsByAddresses(aParticipantStore, aAddresses,
                                       aCreateParticipants, false,
                                       function (participantIds) {
      if (!participantIds) {
        if (DEBUG) debug("findThreadRecordByParticipants: returning null");
        aCallback(null, null);
        return;
      }
      // Find record from thread store.
      let request = aThreadStore.index("participantIds").get(participantIds);
      request.onsuccess = function (event) {
        let threadRecord = event.target.result;
        if (DEBUG) {
          debug("findThreadRecordByParticipants: return "
                + JSON.stringify(threadRecord));
        }
        aCallback(threadRecord, participantIds);
      };
    });
  },

  saveRecord: function saveRecord(aMessageRecord, aAddresses, aCallback) {
    if (aMessageRecord.id === undefined) {
      // Assign a new id.
      this.lastMessageId += 1;
      aMessageRecord.id = this.lastMessageId;
    }
    if (DEBUG) debug("Going to store " + JSON.stringify(aMessageRecord));

    let self = this;
    function notifyResult(rv) {
      if (!aCallback) {
        return;
      }
      let domMessage = self.createDomMessageFromRecord(aMessageRecord);
      aCallback.notify(rv, domMessage);
    }

    this.newTxn(READ_WRITE, function(error, txn, stores) {
      if (error) {
        // TODO bug 832140 check event.target.errorCode
        notifyResult(Cr.NS_ERROR_FAILURE);
        return;
      }
      txn.oncomplete = function oncomplete(event) {
        notifyResult(Cr.NS_OK);
      };
      txn.onabort = function onabort(event) {
        // TODO bug 832140 check event.target.errorCode
        notifyResult(Cr.NS_ERROR_FAILURE);
      };

      let messageStore = stores[0];
      let participantStore = stores[1];
      let threadStore = stores[2];

      self.findThreadRecordByParticipants(threadStore, participantStore,
                                          aAddresses, true,
                                          function (threadRecord,
                                                    participantIds) {
        if (!participantIds) {
          notifyResult(Cr.NS_ERROR_FAILURE);
          return;
        }

        let insertMessageRecord = function (threadId) {
          // Setup threadId & threadIdIndex.
          aMessageRecord.threadId = threadId;
          aMessageRecord.threadIdIndex = [threadId, timestamp];
          // Setup participantIdsIndex.
          aMessageRecord.participantIdsIndex = [];
          for each (let id in participantIds) {
            aMessageRecord.participantIdsIndex.push([id, timestamp]);
          }
          // Really add to message store.
          messageStore.put(aMessageRecord);
        };

        let timestamp = aMessageRecord.timestamp;
        if (threadRecord) {
          let needsUpdate = false;

          if (threadRecord.lastTimestamp <= timestamp) {
            threadRecord.lastTimestamp = timestamp;
            threadRecord.subject = aMessageRecord.body;
            needsUpdate = true;
          }

          if (!aMessageRecord.read) {
            threadRecord.unreadCount++;
            needsUpdate = true;
          }

          if (needsUpdate) {
            threadStore.put(threadRecord);
          }

	  insertMessageRecord(threadRecord.id);
          return;
        }

        threadStore.add({participantIds: participantIds,
                         participantAddresses: aAddresses,
                         lastMessageId: aMessageRecord.id,
                         lastTimestamp: timestamp,
                         subject: aMessageRecord.body,
                         unreadCount: aMessageRecord.read ? 0 : 1})
                   .onsuccess = function (event) {
          let threadId = event.target.result;
          insertMessageRecord(threadId);
        };
      });
    }, [MESSAGE_STORE_NAME, PARTICIPANT_STORE_NAME, THREAD_STORE_NAME]);
    // We return the key that we expect to store in the db
    return aMessageRecord.id;
  },

  getRilIccInfoMsisdn: function getRilIccInfoMsisdn() {
    let iccInfo = this.mRIL.rilContext.iccInfo;
    let number = iccInfo ? iccInfo.msisdn : null;

    // Workaround an xpconnect issue with undefined string objects.
    // See bug 808220
    if (number === undefined || number === "undefined") {
      return null;
    }
    return number;
  },

  /**
   * nsIRilMobileMessageDatabaseService API
   */

  saveReceivedMessage: function saveReceivedMessage(aMessage, aCallback) {
    if (aMessage.type == undefined ||
        aMessage.sender == undefined ||
        (aMessage.type == "sms" && aMessage.messageClass == undefined) ||
        (aMessage.type == "mms" && aMessage.delivery == undefined) ||
        (aMessage.type == "mms" && aMessage.deliveryStatus == undefined) ||
        aMessage.timestamp == undefined) {
      if (aCallback) {
        aCallback.notify(Cr.NS_ERROR_FAILURE, null);
      }
      return;
    }
    let self = this.getRilIccInfoMsisdn();
    let threadParticipants = [aMessage.sender];
    if (aMessage.type == "sms") {
      aMessage.receiver = self;
    } else if (aMessage.type == "mms") {
      if (!aMessage.receivers.length) {
        if (self) {
          aMessage.receivers.push(self);
        }
      } else {
        let slicedReceivers = aMessage.receivers.slice();
        let found = slicedReceivers.indexOf(self);
        if (self && (found !== -1)) {
          slicedReceivers.splice(found, 1);
        }
        threadParticipants = threadParticipants.concat(slicedReceivers);
      }
    }

    let timestamp = aMessage.timestamp;

    // Adding needed indexes and extra attributes for internal use.
    // threadIdIndex & participantIdsIndex are filled in saveRecord().
    aMessage.deliveryIndex = [DELIVERY_RECEIVED, timestamp];
    aMessage.readIndex = [FILTER_READ_UNREAD, timestamp];

    if (aMessage.type == "sms") {
      aMessage.deliveryStatus = DELIVERY_STATUS_SUCCESS;
      aMessage.delivery = DELIVERY_RECEIVED;
    }

    aMessage.read = FILTER_READ_UNREAD;

    return this.saveRecord(aMessage, threadParticipants, aCallback);
  },

  saveSendingMessage: function saveSendingMessage(aMessage, aCallback) {
    if ((aMessage.type != "sms" && aMessage.type != "mms") ||
        (aMessage.type == "sms" && !aMessage.receiver) ||
        (aMessage.type == "mms" && !aMessage.receivers) ||
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
      aMessage.deliveryStatus = [];
      for (let i = 0; i < receivers.length; i++) {
        aMessage.deliveryStatus.push(deliveryStatus);
      }
    }

    aMessage.sender = this.getRilIccInfoMsisdn();
    let timestamp = aMessage.timestamp;

    // Adding needed indexes and extra attributes for internal use.
    // threadIdIndex & participantIdsIndex are filled in saveRecord().
    aMessage.deliveryIndex = [DELIVERY_SENDING, timestamp];
    aMessage.readIndex = [FILTER_READ_READ, timestamp];
    aMessage.delivery = DELIVERY_SENDING;
    aMessage.messageClass = MESSAGE_CLASS_NORMAL;
    aMessage.read = FILTER_READ_READ;

    let addresses;
    if (aMessage.type == "sms") {
      addresses = [aMessage.receiver];
    } else if (aMessage.type == "mms") {
      addresses = aMessage.receivers;
    }
    return this.saveRecord(aMessage, addresses, aCallback);
  },

  setMessageDelivery: function setMessageDelivery(
      messageId, receiver, delivery, deliveryStatus, callback) {
    if (DEBUG) {
      debug("Setting message " + messageId + " delivery to " + delivery
            + ", and deliveryStatus to " + deliveryStatus);
    }

    let self = this;
    let messageRecord;
    function notifyResult(rv) {
      if (!callback) {
        return;
      }
      let domMessage = self.createDomMessageFromRecord(messageRecord);
      callback.notify(rv, domMessage);
    }

    this.newTxn(READ_WRITE, function (error, txn, messageStore) {
      if (error) {
        // TODO bug 832140 check event.target.errorCode
        notifyResult(Cr.NS_ERROR_FAILURE);
        return;
      }
      txn.oncomplete = function oncomplete(event) {
        notifyResult(Cr.NS_OK);
      };
      txn.onabort = function onabort(event) {
        // TODO bug 832140 check event.target.errorCode
        notifyResult(Cr.NS_ERROR_FAILURE);
      };

      let getRequest = messageStore.get(messageId);
      getRequest.onsuccess = function onsuccess(event) {
        messageRecord = event.target.result;
        if (!messageRecord) {
          if (DEBUG) debug("Message ID " + messageId + " not found");
          return;
        }
        if (messageRecord.id != messageId) {
          if (DEBUG) {
            debug("Retrieve message ID (" + messageId + ") is " +
                  "different from the one we got");
          }
          return;
        }

        let isRecordUpdated = false;

        // Update |messageRecord.delivery| if needed.
        if (delivery && messageRecord.delivery != delivery) {
          messageRecord.delivery = delivery;
          messageRecord.deliveryIndex = [delivery, messageRecord.timestamp];
          isRecordUpdated = true;
        }

        // Update |messageRecord.deliveryStatus| if needed.
        if (deliveryStatus) {
          if (messageRecord.type == "sms") {
            if (messageRecord.deliveryStatus != deliveryStatus) {
              messageRecord.deliveryStatus = deliveryStatus;
              isRecordUpdated = true;
            }
          } else if (messageRecord.type == "mms") {
            if (!receiver) {
              for (let i = 0; i < messageRecord.deliveryStatus.length; i++) {
                if (messageRecord.deliveryStatus[i] != deliveryStatus) {
                  messageRecord.deliveryStatus[i] = deliveryStatus;
                  isRecordUpdated = true;
                }
              }
            } else {
              let index = messageRecord.receivers.indexOf(receiver);
              if (index == -1) {
                if (DEBUG) {
                  debug("Cannot find the receiver. Fail to set delivery status.");
                }
                return;
              }
              if (messageRecord.deliveryStatus[index] != deliveryStatus) {
                messageRecord.deliveryStatus[index] = deliveryStatus;
                isRecordUpdated = true;
              }
            }
          }
        }

        // Only updates messages that have different delivery or deliveryStatus.
        if (!isRecordUpdated) {
          if (DEBUG) {
            debug("The values of attribute delivery and deliveryStatus are the"
                  + " the same with given parameters.");
          }
          return;
        }

        if (DEBUG) {
          debug("The record's delivery and/or deliveryStatus are updated.");
        }
        messageStore.put(messageRecord);
      };
    });
  },

  /**
   * nsIMobileMessageDatabaseService API
   */

  getMessage: function getMessage(messageId, aRequest) {
    if (DEBUG) debug("Retrieving message with ID " + messageId);
    let self = this;
    this.newTxn(READ_ONLY, function (error, txn, messageStore) {
      if (error) {
        if (DEBUG) debug(error);
        aRequest.notifyGetMessageFailed(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
        return;
      }
      let request = messageStore.mozGetAll(messageId);

      txn.oncomplete = function oncomplete() {
        if (DEBUG) debug("Transaction " + txn + " completed.");
        if (request.result.length > 1) {
          if (DEBUG) debug("Got too many results for id " + messageId);
          aRequest.notifyGetMessageFailed(Ci.nsIMobileMessageCallback.UNKNOWN_ERROR);
          return;
        }
        let messageRecord = request.result[0];
        if (!messageRecord) {
          if (DEBUG) debug("Message ID " + messageId + " not found");
          aRequest.notifyGetMessageFailed(Ci.nsIMobileMessageCallback.NOT_FOUND_ERROR);
          return;
        }
        if (messageRecord.id != messageId) {
          if (DEBUG) {
            debug("Requested message ID (" + messageId + ") is " +
                  "different from the one we got");
          }
          aRequest.notifyGetMessageFailed(Ci.nsIMobileMessageCallback.UNKNOWN_ERROR);
          return;
        }
        let domMessage = self.createDomMessageFromRecord(messageRecord);
        aRequest.notifyMessageGot(domMessage);
      };

      txn.onerror = function onerror(event) {
        if (DEBUG) {
          if (event.target)
            debug("Caught error on transaction", event.target.errorCode);
        }
        //TODO look at event.target.errorCode, pick appropriate error constant
        aRequest.notifyGetMessageFailed(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
      };
    });
  },

  deleteMessage: function deleteMessage(messageId, aRequest) {
    let deleted = false;
    let self = this;
    this.newTxn(READ_WRITE, function (error, txn, stores) {
      if (error) {
        aRequest.notifyDeleteMessageFailed(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
        return;
      }
      txn.onerror = function onerror(event) {
        if (DEBUG) debug("Caught error on transaction", event.target.errorCode);
        //TODO look at event.target.errorCode, pick appropriate error constant
        aRequest.notifyDeleteMessageFailed(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
      };

      const messageStore = stores[0];
      const threadStore = stores[1];

      let deleted = false;

      txn.oncomplete = function oncomplete(event) {
        if (DEBUG) debug("Transaction " + txn + " completed.");
        aRequest.notifyMessageDeleted(deleted);
      };

      messageStore.get(messageId).onsuccess = function(event) {
        let messageRecord = event.target.result;
        if (messageRecord) {
          if (DEBUG) debug("Deleting message id " + messageId);

          // First actually delete the message.
          messageStore.delete(messageId).onsuccess = function(event) {
            deleted = true;

            // Then update unread count and most recent message.
            let threadId = messageRecord.threadId;

            threadStore.get(threadId).onsuccess = function(event) {
              // This must exist.
              let threadRecord = event.target.result;

              if (!messageRecord.read) {
                threadRecord.unreadCount--;
              }

              if (threadRecord.lastMessageId == messageId) {
                // Check most recent sender/receiver.
                let range = IDBKeyRange.bound([threadId, 0], [threadId, ""]);
                let request = messageStore.index("threadId")
                                          .openCursor(range, PREV);
                request.onsuccess = function(event) {
                  let cursor = event.target.result;
                  if (!cursor) {
                    if (DEBUG) {
                      debug("Deleting mru entry for thread id " + threadId);
                    }
                    threadStore.delete(threadId);
                    return;
                  }

                  let nextMsg = cursor.value;
                  threadRecord.lastMessageId = nextMsg.id;
                  threadRecord.lastTimestamp = nextMsg.timestamp;
                  threadRecord.subject = nextMsg.body;
                  if (DEBUG) {
                    debug("Updating mru entry: " +
                          JSON.stringify(threadRecord));
                  }
                  threadStore.put(threadRecord);
                };
              } else if (!messageRecord.read) {
                // Shortcut, just update the unread count.
                if (DEBUG) {
                  debug("Updating unread count for thread id " + threadId + ": " +
                        (threadRecord.unreadCount + 1) + " -> " +
                        threadRecord.unreadCount);
                }
                threadStore.put(threadRecord);
              }
            };
          };
        } else if (DEBUG) {
          debug("Message id " + messageId + " does not exist");
        }
      };
    }, [MESSAGE_STORE_NAME, THREAD_STORE_NAME]);
  },

  createMessageList: function createMessageList(filter, reverse, aRequest) {
    if (DEBUG) {
      debug("Creating a message list. Filters:" +
            " startDate: " + filter.startDate +
            " endDate: " + filter.endDate +
            " delivery: " + filter.delivery +
            " numbers: " + filter.numbers +
            " read: " + filter.read +
            " reverse: " + reverse);
    }

    let self = this;
    this.newTxn(READ_ONLY, function (error, txn, stores) {
      if (error) {
        //TODO look at event.target.errorCode, pick appropriate error constant.
        if (DEBUG) debug("IDBRequest error " + error.target.errorCode);
        aRequest.notifyReadMessageListFailed(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
        return;
      }

      let messageStore = stores[0];
      let participantStore = stores[1];

      let messageList = {
        listId: -1,
        reverse: reverse,
        processing: true,
        stop: false,
        // Local contexts for multiple filter targets' case.
        contexts: null,
        // Result queues for multiple numbers filter's case.
        numberQueues: null,
        // Pending createMessageList or getNextMessageInList request.
        requestWaiting: aRequest,
        results: []
      };

      let onNextMessageInListGotCb =
        self.onNextMessageInListGot.bind(self, messageStore, messageList);

      let singleFilterSuccessCb = function onsfsuccess(event) {
        if (messageList.stop) {
          // Client called clearMessageList(). Return.
          return;
        }

        let cursor = event.target.result;
        // Once the cursor has retrieved all keys that matches its key range,
        // the filter search is done.
        if (cursor) {
          onNextMessageInListGotCb(cursor.primaryKey);
          cursor.continue();
        } else {
          onNextMessageInListGotCb(0);
        }
      };

      let singleFilterErrorCb = function onsferror(event) {
        if (messageList.stop) {
          // Client called clearMessageList(). Return.
          return;
        }

        if (DEBUG && event) debug("IDBRequest error " + event.target.errorCode);
        onNextMessageInListGotCb(-1);
      };

      let direction = reverse ? PREV : NEXT;

      // We support filtering by date range only (see `else` block below) or
      // by number/delivery status/read status with an optional date range.
      if (filter.delivery || filter.numbers || filter.read != undefined) {
        let multiFiltersGotCb = self.onNextMessageInMultiFiltersGot
                                    .bind(self, messageStore, messageList);

        let multiFiltersSuccessCb = function onmfsuccess(contextIndex, event) {
          if (messageList.stop) {
            // Client called clearMessageList(). Return.
            return;
          }

          let cursor = event.target.result;
          if (cursor) {
            if (multiFiltersGotCb(contextIndex,
                                  cursor.primaryKey, cursor.key[1])) {
              cursor.continue();
            }
          } else {
            multiFiltersGotCb(contextIndex, 0, 0);
          }
        };

        let multiFiltersErrorCb = function onmferror(contextIndex, event) {
          if (messageList.stop) {
            // Client called clearMessageList(). Return.
            return;
          }

          // Act as no more matched records.
          multiFiltersGotCb(contextIndex, 0, 0);
        };

        // Numeric 0 is smaller than any time stamp, and empty string is larger
        // than all numeric values.
        let startDate = 0, endDate = "";
        if (filter.startDate != null) {
          startDate = filter.startDate.getTime();
        }
        if (filter.endDate != null) {
          endDate = filter.endDate.getTime();
        }

        let singleFilter;
        {
          let numberOfContexts = 0;
          if (filter.delivery) numberOfContexts++;
          if (filter.numbers) numberOfContexts++;
          if (filter.read != undefined) numberOfContexts++;
          singleFilter = numberOfContexts == 1;
        }

        if (!singleFilter) {
          messageList.contexts = [];
        }

        let numberOfContexts = 0;

        let createRangedRequest = function crr(indexName, key) {
          let range = IDBKeyRange.bound([key, startDate], [key, endDate]);
          return messageStore.index(indexName).openKeyCursor(range, direction);
        };

        let createSimpleRangedRequest = function csrr(indexName, key, contextIndex) {
          let request = createRangedRequest(indexName, key);
          if (singleFilter) {
            request.onsuccess = singleFilterSuccessCb;
            request.onerror = singleFilterErrorCb;
          } else {
            if (contextIndex == null) {
              contextIndex = numberOfContexts++;
              messageList.contexts.push({
                processing: true,
                results: []
              });
            }
            request.onsuccess = multiFiltersSuccessCb.bind(null, contextIndex);
            request.onerror = multiFiltersErrorCb.bind(null, contextIndex);
          }
        };

        // Retrieve the keys from the 'delivery' index that matches the
        // value of filter.delivery.
        if (filter.delivery) {
          if (DEBUG) debug("filter.delivery " + filter.delivery);
          createSimpleRangedRequest("delivery", filter.delivery);
        }

        // Retrieve the keys from the 'sender' and 'receiver' indexes that
        // match the values of filter.numbers
        if (filter.numbers) {
          if (DEBUG) debug("filter.numbers " + filter.numbers.join(", "));
          let contextIndex;
          if (!singleFilter) {
            contextIndex = numberOfContexts++;
            messageList.contexts.push({
              processing: true,
              results: []
            });
          }
          self.findParticipantIdsByAddresses(participantStore, filter.numbers,
                                             false, true,
                                             function (participantIds) {
            if (!participantIds || !participantIds.length) {
              // Oops! No such participant at all.

              if (messageList.stop) {
                // Client called clearMessageList(). Return.
                return;
              }

              if (singleFilter) {
                onNextMessageInListGotCb(0);
              } else {
                multiFiltersGotCb(contextIndex, 0, 0);
              }
              return;
            }

            if (participantIds.length == 1) {
              createSimpleRangedRequest("participantIds", participantIds[0],
                                        contextIndex);
              return;
            }

            let multiNumbersGotCb =
              self.onNextMessageInMultiNumbersGot
                  .bind(self, messageStore, messageList, contextIndex);

            let multiNumbersSuccessCb = function onmnsuccess(queueIndex, event) {
              if (messageList.stop) {
                // Client called clearMessageList(). Return.
                return;
              }

              let cursor = event.target.result;
              if (cursor) {
                // If queueIndex is non-zero, it's timestamp result queue;
                // otherwise, it's per phone number result queue.
                let key = queueIndex ? cursor.key[1] : cursor.key;
                if (multiNumbersGotCb(queueIndex, cursor.primaryKey, key)) {
                  cursor.continue();
                }
              } else {
                multiNumbersGotCb(queueIndex, 0, 0);
              }
            };

            let multiNumbersErrorCb = function onmnerror(queueIndex, event) {
              if (messageList.stop) {
                // Client called clearMessageList(). Return.
                return;
              }

              // Act as no more matched records.
              multiNumbersGotCb(queueIndex, 0, 0);
            };

            messageList.numberQueues = [{
              // For timestamp.
              processing: 1,
              results: []
            }, {
              // For all numbers.
              processing: participantIds.length,
              results: []
            }];

            let timeRange = null;
            if (filter.startDate != null && filter.endDate != null) {
              timeRange = IDBKeyRange.bound(filter.startDate.getTime(),
                                            filter.endDate.getTime());
            } else if (filter.startDate != null) {
              timeRange = IDBKeyRange.lowerBound(filter.startDate.getTime());
            } else if (filter.endDate != null) {
              timeRange = IDBKeyRange.upperBound(filter.endDate.getTime());
            }

            let timeRequest = messageStore.index("timestamp")
                                          .openKeyCursor(timeRange, direction);
            timeRequest.onsuccess = multiNumbersSuccessCb.bind(null, 0);
            timeRequest.onerror = multiNumbersErrorCb.bind(null, 0);

            for (let i = 0; i < participantIds.length; i++) {
              let request = createRangedRequest("participantIds",
                                                participantIds[i]);
              request.onsuccess = multiNumbersSuccessCb.bind(null, 1);
              request.onerror = multiNumbersErrorCb.bind(null, 1);
            }
          });
        }

        // Retrieve the keys from the 'read' index that matches the value of
        // filter.read
        if (filter.read != undefined) {
          let read = filter.read ? FILTER_READ_READ : FILTER_READ_UNREAD;
          if (DEBUG) debug("filter.read " + read);
          createSimpleRangedRequest("read", read);
        }
      } else {
        // Filtering by date range only.
        if (DEBUG) {
          debug("filter.timestamp " + filter.startDate + ", " + filter.endDate);
        }

        let range = null;
        if (filter.startDate != null && filter.endDate != null) {
          range = IDBKeyRange.bound(filter.startDate.getTime(),
                                    filter.endDate.getTime());
        } else if (filter.startDate != null) {
          range = IDBKeyRange.lowerBound(filter.startDate.getTime());
        } else if (filter.endDate != null) {
          range = IDBKeyRange.upperBound(filter.endDate.getTime());
        }

        let request = messageStore.index("timestamp").openKeyCursor(range, direction);
        request.onsuccess = singleFilterSuccessCb;
        request.onerror = singleFilterErrorCb;
      }

      if (DEBUG) {
        txn.oncomplete = function oncomplete(event) {
          debug("Transaction " + txn + " completed.");
        };
      }

      txn.onerror = singleFilterErrorCb;
    }, [MESSAGE_STORE_NAME, PARTICIPANT_STORE_NAME]);
  },

  getNextMessageInList: function getNextMessageInList(listId, aRequest) {
    if (DEBUG) debug("Getting next message in list " + listId);
    let messageId;
    let list = this.messageLists[listId];
    if (!list) {
      if (DEBUG) debug("Wrong list id");
      aRequest.notifyReadMessageListFailed(Ci.nsIMobileMessageCallback.NOT_FOUND_ERROR);
      return;
    }
    if (list.processing) {
      // Database transaction ongoing, let it reply for us so that we won't get
      // blocked by the existing transaction.
      if (list.requestWaiting) {
        if (DEBUG) debug("Already waiting for another request!");
        return;
      }
      list.requestWaiting = aRequest;
      return;
    }
    if (!list.results.length) {
      if (DEBUG) debug("Reached the end of the list!");
      aRequest.notifyNoMessageInList();
      return;
    }
    if (list.results[0] < 0) {
      aRequest.notifyReadMessageListFailed(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
      return;
    }
    messageId = list.results.shift();
    let self = this;
    this.newTxn(READ_ONLY, function (error, txn, messageStore) {
      if (DEBUG) debug("Fetching message " + messageId);
      let request = messageStore.get(messageId);
      let messageRecord;
      request.onsuccess = function onsuccess(event) {
        messageRecord = request.result;
      };

      txn.oncomplete = function oncomplete(event) {
        if (DEBUG) debug("Transaction " + txn + " completed.");
        if (!messageRecord) {
          if (DEBUG) debug("Could not get message id " + messageId);
          aRequest.notifyReadMessageListFailed(Ci.nsIMobileMessageCallback.NOT_FOUND_ERROR);
        }
        let domMessage = self.createDomMessageFromRecord(messageRecord);
        aRequest.notifyNextMessageInListGot(domMessage);
      };

      txn.onerror = function onerror(event) {
        //TODO check event.target.errorCode
        if (DEBUG) {
          debug("Error retrieving message id: " + messageId +
                ". Error code: " + event.target.errorCode);
        }
        aRequest.notifyReadMessageListFailed(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
      };
    });
  },

  clearMessageList: function clearMessageList(listId) {
    if (DEBUG) debug("Clearing message list: " + listId);
    if (this.messageLists[listId]) {
      this.messageLists[listId].stop = true;
      delete this.messageLists[listId];
    }
  },

  markMessageRead: function markMessageRead(messageId, value, aRequest) {
    if (DEBUG) debug("Setting message " + messageId + " read to " + value);
    this.newTxn(READ_WRITE, function (error, txn, stores) {
      if (error) {
        if (DEBUG) debug(error);
        aRequest.notifyMarkMessageReadFailed(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
        return;
      }
      txn.onerror = function onerror(event) {
        if (DEBUG) debug("Caught error on transaction ", event.target.errorCode);
        aRequest.notifyMarkMessageReadFailed(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
      };
      let messageStore = stores[0];
      let threadStore = stores[1];
      messageStore.get(messageId).onsuccess = function onsuccess(event) {
        let messageRecord = event.target.result;
        if (!messageRecord) {
          if (DEBUG) debug("Message ID " + messageId + " not found");
          aRequest.notifyMarkMessageReadFailed(Ci.nsIMobileMessageCallback.NOT_FOUND_ERROR);
          return;
        }
        if (messageRecord.id != messageId) {
          if (DEBUG) {
            debug("Retrieve message ID (" + messageId + ") is " +
                  "different from the one we got");
          }
          aRequest.notifyMarkMessageReadFailed(Ci.nsIMobileMessageCallback.UNKNOWN_ERROR);
          return;
        }
        // If the value to be set is the same as the current message `read`
        // value, we just notify successfully.
        if (messageRecord.read == value) {
          if (DEBUG) debug("The value of messageRecord.read is already " + value);
          aRequest.notifyMessageMarkedRead(messageRecord.read);
          return;
        }
        messageRecord.read = value ? FILTER_READ_READ : FILTER_READ_UNREAD;
        messageRecord.readIndex = [messageRecord.read, messageRecord.timestamp];
        if (DEBUG) debug("Message.read set to: " + value);
        messageStore.put(messageRecord).onsuccess = function onsuccess(event) {
          if (DEBUG) {
            debug("Update successfully completed. Message: " +
                  JSON.stringify(event.target.result));
          }

          // Now update the unread count.
          let threadId = messageRecord.threadId;

          threadStore.get(threadId).onsuccess = function(event) {
            let threadRecord = event.target.result;
            threadRecord.unreadCount += value ? -1 : 1;
            if (DEBUG) {
              debug("Updating unreadCount for thread id " + threadId + ": " +
                    (value ?
                     threadRecord.unreadCount + 1 :
                     threadRecord.unreadCount - 1) +
                     " -> " + threadRecord.unreadCount);
            }
            threadStore.put(threadRecord).onsuccess = function(event) {
              aRequest.notifyMessageMarkedRead(messageRecord.read);
            };
          };
        };
      };
    }, [MESSAGE_STORE_NAME, THREAD_STORE_NAME]);
  },

  getThreadList: function getThreadList(aRequest) {
    if (DEBUG) debug("Getting thread list");
    this.newTxn(READ_ONLY, function (error, txn, threadStore) {
      if (error) {
        if (DEBUG) debug(error);
        aRequest.notifyThreadListFailed(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
        return;
      }
      txn.onerror = function onerror(event) {
        if (DEBUG) debug("Caught error on transaction ", event.target.errorCode);
        aRequest.notifyThreadListFailed(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
      };
      let request = threadStore.index("lastTimestamp").mozGetAll();
      request.onsuccess = function(event) {
        // TODO: keep backward compatibility of original API interface only.
        let results = [];
        for each (let item in event.target.result) {
          results.push({
            senderOrReceiver: item.participantAddresses[0],
            timestamp: item.lastTimestamp,
            body: item.subject,
            unreadCount: item.unreadCount
          });
        }
        aRequest.notifyThreadList(results);
      };
    }, [THREAD_STORE_NAME]);
  }
};

XPCOMUtils.defineLazyServiceGetter(MobileMessageDatabaseService.prototype, "mRIL",
                                   "@mozilla.org/ril;1",
                                   "nsIRadioInterfaceLayer");

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MobileMessageDatabaseService]);

function debug() {
  dump("MobileMessageDatabaseService: " + Array.slice(arguments).join(" ") + "\n");
}
