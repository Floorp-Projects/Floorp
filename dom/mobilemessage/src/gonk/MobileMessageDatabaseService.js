/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PhoneNumberUtils.jsm");
Cu.importGlobalProperties(["indexedDB"]);

var RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

const RIL_MOBILEMESSAGEDATABASESERVICE_CONTRACTID =
  "@mozilla.org/mobilemessage/rilmobilemessagedatabaseservice;1";
const RIL_MOBILEMESSAGEDATABASESERVICE_CID =
  Components.ID("{29785f90-6b5b-11e2-9201-3b280170b2ec}");
const RIL_GETMESSAGESCURSOR_CID =
  Components.ID("{484d1ad8-840e-4782-9dc4-9ebc4d914937}");
const RIL_GETTHREADSCURSOR_CID =
  Components.ID("{95ee7c3e-d6f2-4ec4-ade5-0c453c036d35}");

const DEBUG = false;
const DISABLE_MMS_GROUPING_FOR_RECEIVING = true;


const DB_NAME = "sms";
const DB_VERSION = 19;
const MESSAGE_STORE_NAME = "sms";
const THREAD_STORE_NAME = "thread";
const PARTICIPANT_STORE_NAME = "participant";
const MOST_RECENT_STORE_NAME = "most-recent";

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

const COLLECT_ID_END = 0;
const COLLECT_ID_ERROR = -1;
const COLLECT_TIMESTAMP_UNUSED = 0;

XPCOMUtils.defineLazyServiceGetter(this, "gMobileMessageService",
                                   "@mozilla.org/mobilemessage/mobilemessageservice;1",
                                   "nsIMobileMessageService");

XPCOMUtils.defineLazyServiceGetter(this, "gMMSService",
                                   "@mozilla.org/mms/rilmmsservice;1",
                                   "nsIMmsService");

XPCOMUtils.defineLazyGetter(this, "MMS", function () {
  let MMS = {};
  Cu.import("resource://gre/modules/MmsPduHelper.jsm", MMS);
  return MMS;
});

/**
 * MobileMessageDatabaseService
 */
function MobileMessageDatabaseService() {
  // Prime the directory service's cache to ensure that the ProfD entry exists
  // by the time IndexedDB queries for it off the main thread. (See bug 743635.)
  Services.dirsvc.get("ProfD", Ci.nsIFile);

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
  this.updatePendingTransactionToError();
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

    let request = indexedDB.open(DB_NAME, DB_VERSION);
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

      function update(currentVersion) {
        let next = update.bind(self, currentVersion + 1);

        switch (currentVersion) {
          case 0:
            if (DEBUG) debug("New database");
            self.createSchema(db, next);
            break;
          case 1:
            if (DEBUG) debug("Upgrade to version 2. Including `read` index");
            self.upgradeSchema(event.target.transaction, next);
            break;
          case 2:
            if (DEBUG) debug("Upgrade to version 3. Fix existing entries.");
            self.upgradeSchema2(event.target.transaction, next);
            break;
          case 3:
            if (DEBUG) debug("Upgrade to version 4. Add quick threads view.");
            self.upgradeSchema3(db, event.target.transaction, next);
            break;
          case 4:
            if (DEBUG) debug("Upgrade to version 5. Populate quick threads view.");
            self.upgradeSchema4(event.target.transaction, next);
            break;
          case 5:
            if (DEBUG) debug("Upgrade to version 6. Use PhonenumberJS.");
            self.upgradeSchema5(event.target.transaction, next);
            break;
          case 6:
            if (DEBUG) debug("Upgrade to version 7. Use multiple entry indexes.");
            self.upgradeSchema6(event.target.transaction, next);
            break;
          case 7:
            if (DEBUG) debug("Upgrade to version 8. Add participant/thread stores.");
            self.upgradeSchema7(db, event.target.transaction, next);
            break;
          case 8:
            if (DEBUG) debug("Upgrade to version 9. Add transactionId index for incoming MMS.");
            self.upgradeSchema8(event.target.transaction, next);
            break;
          case 9:
            if (DEBUG) debug("Upgrade to version 10. Upgrade type if it's not existing.");
            self.upgradeSchema9(event.target.transaction, next);
            break;
          case 10:
            if (DEBUG) debug("Upgrade to version 11. Add last message type into threadRecord.");
            self.upgradeSchema10(event.target.transaction, next);
            break;
          case 11:
            if (DEBUG) debug("Upgrade to version 12. Add envelopeId index for outgoing MMS.");
            self.upgradeSchema11(event.target.transaction, next);
            break;
          case 12:
            if (DEBUG) debug("Upgrade to version 13. Replaced deliveryStatus by deliveryInfo.");
            self.upgradeSchema12(event.target.transaction, next);
            break;
          case 13:
            if (DEBUG) debug("Upgrade to version 14. Fix the wrong participants.");
            self.upgradeSchema13(event.target.transaction, next);
            break;
          case 14:
            if (DEBUG) debug("Upgrade to version 15. Add deliveryTimestamp.");
            self.upgradeSchema14(event.target.transaction, next);
            break;
          case 15:
            if (DEBUG) debug("Upgrade to version 16. Add ICC ID for each message.");
            self.upgradeSchema15(event.target.transaction, next);
            break;
          case 16:
            if (DEBUG) debug("Upgrade to version 17. Add isReadReportSent for incoming MMS.");
            self.upgradeSchema16(event.target.transaction, next);
            break;
          case 17:
            if (DEBUG) debug("Upgrade to version 18. Add last message subject into threadRecord.");
            self.upgradeSchema17(event.target.transaction, next);
            break;
          case 18:
            if (DEBUG) debug("Upgrade to version 19. Add pid for incoming SMS.");
            self.upgradeSchema18(event.target.transaction, next);
            break;
          case 19:
            // This will need to be moved for each new version
            if (DEBUG) debug("Upgrade finished.");
            break;
          default:
            event.target.transaction.abort();
            callback("Old database version: " + event.oldVersion, null);
            break;
        }
      }

      update(currentVersion);
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
   * Sometimes user might reboot or remove battery while sending/receiving
   * message. This is function set the status of message records to error.
   */
  updatePendingTransactionToError: function updatePendingTransactionToError() {
    this.newTxn(READ_WRITE, function (error, txn, messageStore) {
      if (DEBUG) {
        txn.onerror = function onerror(event) {
          debug("updatePendingTransactionToError fail, event = " + event);
        };
      }

      let deliveryIndex = messageStore.index("delivery");

      // Set all 'delivery: sending' records to 'delivery: error' and 'deliveryStatus:
      // error'.
      let keyRange = IDBKeyRange.bound([DELIVERY_SENDING, 0], [DELIVERY_SENDING, ""]);
      let cursorRequestSending = deliveryIndex.openCursor(keyRange);
      cursorRequestSending.onsuccess = function(event) {
        let messageCursor = event.target.result;
        if (!messageCursor) {
          return;
        }

        let messageRecord = messageCursor.value;

        // Set delivery to error.
        messageRecord.delivery = DELIVERY_ERROR;
        messageRecord.deliveryIndex = [DELIVERY_ERROR, messageRecord.timestamp];

        if (messageRecord.type == "sms") {
          messageRecord.deliveryStatus = DELIVERY_STATUS_ERROR;
        } else {
          // Set delivery status to error.
          for (let i = 0; i < messageRecord.deliveryInfo.length; i++) {
            messageRecord.deliveryInfo[i].deliveryStatus = DELIVERY_STATUS_ERROR;
          }
        }

        messageCursor.update(messageRecord);
        messageCursor.continue();
      };

      // Set all 'delivery: not-downloaded' and 'deliveryStatus: pending'
      // records to 'delivery: not-downloaded' and 'deliveryStatus: error'.
      keyRange = IDBKeyRange.bound([DELIVERY_NOT_DOWNLOADED, 0], [DELIVERY_NOT_DOWNLOADED, ""]);
      let cursorRequestNotDownloaded = deliveryIndex.openCursor(keyRange);
      cursorRequestNotDownloaded.onsuccess = function(event) {
        let messageCursor = event.target.result;
        if (!messageCursor) {
          return;
        }

        let messageRecord = messageCursor.value;

        // We have no "not-downloaded" SMS messages.
        if (messageRecord.type == "sms") {
          messageCursor.continue();
          return;
        }

        // Set delivery status to error.
        let deliveryInfo = messageRecord.deliveryInfo;
        if (deliveryInfo.length == 1 &&
            deliveryInfo[0].deliveryStatus == DELIVERY_STATUS_PENDING) {
          deliveryInfo[0].deliveryStatus = DELIVERY_STATUS_ERROR;
        }

        messageCursor.update(messageRecord);
        messageCursor.continue();
      };
    });
  },

  /**
   * Create the initial database schema.
   *
   * TODO need to worry about number normalization somewhere...
   * TODO full text search on body???
   */
  createSchema: function createSchema(db, next) {
    // This messageStore holds the main mobile message data.
    let messageStore = db.createObjectStore(MESSAGE_STORE_NAME, { keyPath: "id" });
    messageStore.createIndex("timestamp", "timestamp", { unique: false });
    if (DEBUG) debug("Created object stores and indexes");
    next();
  },

  /**
   * Upgrade to the corresponding database schema version.
   */
  upgradeSchema: function upgradeSchema(transaction, next) {
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);
    messageStore.createIndex("read", "read", { unique: false });
    next();
  },

  upgradeSchema2: function upgradeSchema2(transaction, next) {
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);
    messageStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (!cursor) {
        next();
        return;
      }

      let messageRecord = cursor.value;
      messageRecord.messageClass = MESSAGE_CLASS_NORMAL;
      messageRecord.deliveryStatus = DELIVERY_STATUS_NOT_APPLICABLE;
      cursor.update(messageRecord);
      cursor.continue();
    };
  },

  upgradeSchema3: function upgradeSchema3(db, transaction, next) {
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
    next();
  },

  upgradeSchema4: function upgradeSchema4(transaction, next) {
    let threads = {};
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);
    let mostRecentStore = transaction.objectStore(MOST_RECENT_STORE_NAME);

    messageStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (!cursor) {
        for (let thread in threads) {
          mostRecentStore.put(threads[thread]);
        }
        next();
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

  upgradeSchema5: function upgradeSchema5(transaction, next) {
    // Don't perform any upgrade. See Bug 819560.
    next();
  },

  upgradeSchema6: function upgradeSchema6(transaction, next) {
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
        next();
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
  upgradeSchema7: function upgradeSchema7(db, transaction, next) {
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
        next();
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

  /**
   * Add transactionId index for MMS.
   */
  upgradeSchema8: function upgradeSchema8(transaction, next) {
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);

    // Delete "transactionId" index.
    if (messageStore.indexNames.contains("transactionId")) {
      messageStore.deleteIndex("transactionId");
    }

    // Create new "transactionId" indexes.
    messageStore.createIndex("transactionId", "transactionIdIndex", { unique: true });

    // Populate new "transactionIdIndex" attributes.
    messageStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (!cursor) {
        next();
        return;
      }

      let messageRecord = cursor.value;
      if ("mms" == messageRecord.type &&
          (DELIVERY_NOT_DOWNLOADED == messageRecord.delivery ||
           DELIVERY_RECEIVED == messageRecord.delivery)) {
        messageRecord.transactionIdIndex =
          messageRecord.headers["x-mms-transaction-id"];
        cursor.update(messageRecord);
      }
      cursor.continue();
    };
  },

  upgradeSchema9: function upgradeSchema9(transaction, next) {
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);

    // Update type attributes.
    messageStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (!cursor) {
        next();
        return;
      }

      let messageRecord = cursor.value;
      if (messageRecord.type == undefined) {
        messageRecord.type = "sms";
        cursor.update(messageRecord);
      }
      cursor.continue();
    };
  },

  upgradeSchema10: function upgradeSchema10(transaction, next) {
    let threadStore = transaction.objectStore(THREAD_STORE_NAME);

    // Add 'lastMessageType' to each thread record.
    threadStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (!cursor) {
        next();
        return;
      }

      let threadRecord = cursor.value;
      let lastMessageId = threadRecord.lastMessageId;
      let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);
      let request = messageStore.mozGetAll(lastMessageId);

      request.onsuccess = function onsuccess() {
        let messageRecord = request.result[0];
        if (!messageRecord) {
          if (DEBUG) debug("Message ID " + lastMessageId + " not found");
          return;
        }
        if (messageRecord.id != lastMessageId) {
          if (DEBUG) {
            debug("Requested message ID (" + lastMessageId + ") is different from" +
                  " the one we got");
          }
          return;
        }
        threadRecord.lastMessageType = messageRecord.type;
        cursor.update(threadRecord);
        cursor.continue();
      };

      request.onerror = function onerror(event) {
        if (DEBUG) {
          if (event.target) {
            debug("Caught error on transaction", event.target.errorCode);
          }
        }
        cursor.continue();
      };
    };
  },

  /**
   * Add envelopeId index for MMS.
   */
  upgradeSchema11: function upgradeSchema11(transaction, next) {
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);

    // Delete "envelopeId" index.
    if (messageStore.indexNames.contains("envelopeId")) {
      messageStore.deleteIndex("envelopeId");
    }

    // Create new "envelopeId" indexes.
    messageStore.createIndex("envelopeId", "envelopeIdIndex", { unique: true });

    // Populate new "envelopeIdIndex" attributes.
    messageStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (!cursor) {
        next();
        return;
      }

      let messageRecord = cursor.value;
      if (messageRecord.type == "mms" &&
          messageRecord.delivery == DELIVERY_SENT) {
        messageRecord.envelopeIdIndex = messageRecord.headers["message-id"];
        cursor.update(messageRecord);
      }
      cursor.continue();
    };
  },

  /**
   * Replace deliveryStatus by deliveryInfo.
   */
  upgradeSchema12: function upgradeSchema12(transaction, next) {
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);

    messageStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (!cursor) {
        next();
        return;
      }

      let messageRecord = cursor.value;
      if (messageRecord.type == "mms") {
        messageRecord.deliveryInfo = [];

        if (messageRecord.deliveryStatus.length == 1 &&
            (messageRecord.delivery == DELIVERY_NOT_DOWNLOADED ||
             messageRecord.delivery == DELIVERY_RECEIVED)) {
          messageRecord.deliveryInfo.push({
            receiver: null,
            deliveryStatus: messageRecord.deliveryStatus[0] });
        } else {
          for (let i = 0; i < messageRecord.deliveryStatus.length; i++) {
            messageRecord.deliveryInfo.push({
              receiver: messageRecord.receivers[i],
              deliveryStatus: messageRecord.deliveryStatus[i] });
          }
        }
        delete messageRecord.deliveryStatus;
        cursor.update(messageRecord);
      }
      cursor.continue();
    };
  },

  /**
   * Fix the wrong participants.
   */
  upgradeSchema13: function upgradeSchema13(transaction, next) {
    let participantStore = transaction.objectStore(PARTICIPANT_STORE_NAME);
    let threadStore = transaction.objectStore(THREAD_STORE_NAME);
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);
    let self = this;

    let isInvalid = function (participantRecord) {
      let entries = [];
      for (let addr of participantRecord.addresses) {
        entries.push({
          normalized: addr,
          parsed: PhoneNumberUtils.parseWithMCC(addr, null)
        })
      }
      for (let ix = 0 ; ix < entries.length - 1; ix++) {
        let entry1 = entries[ix];
        for (let iy = ix + 1 ; iy < entries.length; iy ++) {
          let entry2 = entries[iy];
          if (!self.matchPhoneNumbers(entry1.normalized, entry1.parsed,
                                      entry2.normalized, entry2.parsed)) {
            return true;
          }
        }
      }
      return false;
    };

    let invalidParticipantIds = [];
    participantStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (cursor) {
        let participantRecord = cursor.value;
        // Check if this participant record is valid
        if (isInvalid(participantRecord)) {
          invalidParticipantIds.push(participantRecord.id);
          cursor.delete();
        }
        cursor.continue();
        return;
      }

      // Participant store cursor iteration done.
      if (!invalidParticipantIds.length) {
        next();
      }

      // Find affected thread.
      let wrongThreads = [];
      threadStore.openCursor().onsuccess = function(event) {
        let threadCursor = event.target.result;
        if (threadCursor) {
          let threadRecord = threadCursor.value;
          let participantIds = threadRecord.participantIds;
          let foundInvalid = false;
          for (let invalidParticipantId of invalidParticipantIds) {
            if (participantIds.indexOf(invalidParticipantId) != -1) {
              foundInvalid = true;
              break;
            }
          }
          if (foundInvalid) {
            wrongThreads.push(threadRecord.id);
            threadCursor.delete();
          }
          threadCursor.continue();
          return;
        }

        if (!wrongThreads.length) {
          next();
          return;
        }
        // Use recursive function to avoid we add participant twice.
        (function createUpdateThreadAndParticipant(ix) {
          let threadId = wrongThreads[ix];
          let range = IDBKeyRange.bound([threadId, 0], [threadId, ""]);
          messageStore.index("threadId").openCursor(range).onsuccess = function(event) {
            let messageCursor = event.target.result;
            if (!messageCursor) {
              ix++;
              if (ix === wrongThreads.length) {
                next();
                return;
              }
              createUpdateThreadAndParticipant(ix);
              return;
            }

            let messageRecord = messageCursor.value;
            let timestamp = messageRecord.timestamp;
            let threadParticipants = [];
            // Recaculate the thread participants of received message.
            if (messageRecord.delivery === DELIVERY_RECEIVED ||
                messageRecord.delivery === DELIVERY_NOT_DOWNLOADED) {
              threadParticipants.push(messageRecord.sender);
              if (messageRecord.type == "mms") {
                this.fillReceivedMmsThreadParticipants(messageRecord, threadParticipants);
              }
            }
            // Recaculate the thread participants of sent messages and error
            // messages. In error sms messages, we don't have error received sms.
            // In received MMS, we don't update the error to deliver field but
            // deliverStatus. So we only consider sent message in DELIVERY_ERROR.
            else if (messageRecord.delivery === DELIVERY_SENT ||
                messageRecord.delivery === DELIVERY_ERROR) {
              if (messageRecord.type == "sms") {
                threadParticipants = [messageRecord.receiver];
              } else if (messageRecord.type == "mms") {
                threadParticipants = messageRecord.receivers;
              }
            }
            self.findThreadRecordByParticipants(threadStore, participantStore,
                                                threadParticipants, true,
                                                function (threadRecord,
                                                          participantIds) {
              if (!participantIds) {
                debug("participantIds is empty!");
                return;
              }

              let timestamp = messageRecord.timestamp;
              // Setup participantIdsIndex.
              messageRecord.participantIdsIndex = [];
              for each (let id in participantIds) {
                messageRecord.participantIdsIndex.push([id, timestamp]);
              }
              if (threadRecord) {
                let needsUpdate = false;

                if (threadRecord.lastTimestamp <= timestamp) {
                  threadRecord.lastTimestamp = timestamp;
                  threadRecord.subject = messageRecord.body;
                  threadRecord.lastMessageId = messageRecord.id;
                  threadRecord.lastMessageType = messageRecord.type;
                  needsUpdate = true;
                }

                if (!messageRecord.read) {
                  threadRecord.unreadCount++;
                  needsUpdate = true;
                }

                if (needsUpdate) {
                  threadStore.put(threadRecord);
                }
                messageRecord.threadId = threadRecord.id;
                messageRecord.threadIdIndex = [threadRecord.id, timestamp];
                messageCursor.update(messageRecord);
                messageCursor.continue();
                return;
              }

              let threadRecord = {
                participantIds: participantIds,
                participantAddresses: threadParticipants,
                lastMessageId: messageRecord.id,
                lastTimestamp: timestamp,
                subject: messageRecord.body,
                unreadCount: messageRecord.read ? 0 : 1,
                lastMessageType: messageRecord.type
              };
              threadStore.add(threadRecord).onsuccess = function (event) {
                let threadId = event.target.result;
                // Setup threadId & threadIdIndex.
                messageRecord.threadId = threadId;
                messageRecord.threadIdIndex = [threadId, timestamp];
                messageCursor.update(messageRecord);
                messageCursor.continue();
              };
            });
          };
        })(0);
      };
    };
  },

  /**
   * Add deliveryTimestamp.
   */
  upgradeSchema14: function upgradeSchema14(transaction, next) {
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);

    messageStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (!cursor) {
        next();
        return;
      }

      let messageRecord = cursor.value;
      if (messageRecord.type == "sms") {
        messageRecord.deliveryTimestamp = 0;
      } else if (messageRecord.type == "mms") {
        let deliveryInfo = messageRecord.deliveryInfo;
        for (let i = 0; i < deliveryInfo.length; i++) {
          deliveryInfo[i].deliveryTimestamp = 0;
        }
      }
      cursor.update(messageRecord);
      cursor.continue();
    };
  },

  /**
   * Add ICC ID.
   */
  upgradeSchema15: function upgradeSchema15(transaction, next) {
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);
    messageStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (!cursor) {
        next();
        return;
      }

      let messageRecord = cursor.value;
      messageRecord.iccId = null;
      cursor.update(messageRecord);
      cursor.continue();
    };
  },

  /**
   * Add isReadReportSent for incoming MMS.
   */
  upgradeSchema16: function upgradeSchema16(transaction, next) {
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);

    // Update type attributes.
    messageStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (!cursor) {
        next();
        return;
      }

      let messageRecord = cursor.value;
      if (messageRecord.type == "mms") {
        messageRecord.isReadReportSent = false;
        cursor.update(messageRecord);
      }
      cursor.continue();
    };
  },

  upgradeSchema17: function upgradeSchema17(transaction, next) {
    let threadStore = transaction.objectStore(THREAD_STORE_NAME);
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);

    // Add 'lastMessageSubject' to each thread record.
    threadStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (!cursor) {
        next();
        return;
      }

      let threadRecord = cursor.value;
      // We have defined 'threadRecord.subject' in upgradeSchema7(), but it
      // actually means 'threadRecord.body'.  Swap the two values first.
      threadRecord.body = threadRecord.subject;
      delete threadRecord.subject;

      // Only MMS supports subject so assign null for non-MMS one.
      if (threadRecord.lastMessageType != "mms") {
        threadRecord.lastMessageSubject = null;
        cursor.update(threadRecord);

        cursor.continue();
        return;
      }

      messageStore.get(threadRecord.lastMessageId).onsuccess = function(event) {
        let messageRecord = event.target.result;
        let subject = messageRecord.headers.subject;
        threadRecord.lastMessageSubject = subject || null;
        cursor.update(threadRecord);

        cursor.continue();
      };
    };
  },

  /**
   * Add pid for incoming SMS.
   */
  upgradeSchema18: function upgradeSchema18(transaction, next) {
    let messageStore = transaction.objectStore(MESSAGE_STORE_NAME);

    messageStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (!cursor) {
        next();
        return;
      }

      let messageRecord = cursor.value;
      if (messageRecord.type == "sms") {
        messageRecord.pid = RIL.PDU_PID_DEFAULT;
        cursor.update(messageRecord);
      }
      cursor.continue();
    };
  },

  matchParsedPhoneNumbers: function matchParsedPhoneNumbers(addr1, parsedAddr1,
                                                            addr2, parsedAddr2) {
    if ((parsedAddr1.internationalNumber &&
         parsedAddr1.internationalNumber === parsedAddr2.internationalNumber) ||
        (parsedAddr1.nationalNumber &&
         parsedAddr1.nationalNumber === parsedAddr2.nationalNumber)) {
      return true;
    }

    if (parsedAddr1.countryName != parsedAddr2.countryName) {
      return false;
    }

    let ssPref = "dom.phonenumber.substringmatching." + parsedAddr1.countryName;
    if (Services.prefs.getPrefType(ssPref) != Ci.nsIPrefBranch.PREF_INT) {
      return false;
    }

    let val = Services.prefs.getIntPref(ssPref);
    return addr1.length > val &&
           addr2.length > val &&
           addr1.slice(-val) === addr2.slice(-val);
  },

  matchPhoneNumbers: function matchPhoneNumbers(addr1, parsedAddr1, addr2, parsedAddr2) {
    if (parsedAddr1 && parsedAddr2) {
      return this.matchParsedPhoneNumbers(addr1, parsedAddr1, addr2, parsedAddr2);
    }

    if (parsedAddr1) {
      parsedAddr2 = PhoneNumberUtils.parseWithCountryName(addr2, parsedAddr1.countryName);
      if (parsedAddr2) {
        return this.matchParsedPhoneNumbers(addr1, parsedAddr1, addr2, parsedAddr2);
      }

      return false;
    }

    if (parsedAddr2) {
      parsedAddr1 = PhoneNumberUtils.parseWithCountryName(addr1, parsedAddr2.countryName);
      if (parsedAddr1) {
        return this.matchParsedPhoneNumbers(addr1, parsedAddr1, addr2, parsedAddr2);
      }
    }

    return false;
  },

  createDomMessageFromRecord: function createDomMessageFromRecord(aMessageRecord) {
    if (DEBUG) {
      debug("createDomMessageFromRecord: " + JSON.stringify(aMessageRecord));
    }
    if (aMessageRecord.type == "sms") {
      return gMobileMessageService.createSmsMessage(aMessageRecord.id,
                                                    aMessageRecord.threadId,
                                                    aMessageRecord.iccId,
                                                    aMessageRecord.delivery,
                                                    aMessageRecord.deliveryStatus,
                                                    aMessageRecord.sender,
                                                    aMessageRecord.receiver,
                                                    aMessageRecord.body,
                                                    aMessageRecord.messageClass,
                                                    aMessageRecord.timestamp,
                                                    aMessageRecord.deliveryTimestamp,
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
          // Sometimes the part is incomplete because the device reboots when
          // downloading MMS. Don't need to expose this part to the content.
          if (!part) {
            continue;
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
      let expiryDate = 0;
      if (headers["x-mms-expiry"] != undefined) {
        expiryDate = aMessageRecord.timestamp + headers["x-mms-expiry"] * 1000;
      }
      let readReportRequested = headers["x-mms-read-report"] || false;
      return gMobileMessageService.createMmsMessage(aMessageRecord.id,
                                                    aMessageRecord.threadId,
                                                    aMessageRecord.iccId,
                                                    aMessageRecord.delivery,
                                                    aMessageRecord.deliveryInfo,
                                                    aMessageRecord.sender,
                                                    aMessageRecord.receivers,
                                                    aMessageRecord.timestamp,
                                                    aMessageRecord.read,
                                                    subject,
                                                    smil,
                                                    attachments,
                                                    expiryDate,
                                                    readReportRequested);
    }
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

    // Normalize address before searching for participant record.
    let normalizedAddress = PhoneNumberUtils.normalize(aAddress, false);
    let allPossibleAddresses = [normalizedAddress];
    let parsedAddress = PhoneNumberUtils.parse(normalizedAddress);
    if (parsedAddress && parsedAddress.internationalNumber &&
        allPossibleAddresses.indexOf(parsedAddress.internationalNumber) < 0) {
      // We only stores international numbers into participant store because
      // the parsed national number doesn't contain country info and may
      // duplicate in different country.
      allPossibleAddresses.push(parsedAddress.internationalNumber);
    }
    if (DEBUG) {
      debug("findParticipantRecordByAddress: allPossibleAddresses = " +
            JSON.stringify(allPossibleAddresses));
    }

    // Make a copy here because we may need allPossibleAddresses again.
    let needles = allPossibleAddresses.slice(0);
    let request = aParticipantStore.index("addresses").get(needles.pop());
    request.onsuccess = (function onsuccess(event) {
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

      // Try next possible address again.
      if (needles.length) {
        let request = aParticipantStore.index("addresses").get(needles.pop());
        request.onsuccess = onsuccess.bind(this);
        return;
      }

      // 2) Traverse throught all participants and check all alias addresses.
      aParticipantStore.openCursor().onsuccess = (function (event) {
        let cursor = event.target.result;
        if (!cursor) {
          // Have traversed whole object store but still in vain.
          if (!aCreate) {
            aCallback(null);
            return;
          }

          let participantRecord = { addresses: [normalizedAddress] };
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
        for (let storedAddress of participantRecord.addresses) {
          let parsedStoredAddress = PhoneNumberUtils.parseWithMCC(storedAddress, null);
          let match = this.matchPhoneNumbers(normalizedAddress, parsedAddress,
                                             storedAddress, parsedStoredAddress);
          if (!match) {
            // 3) Else we fail to match current stored participant record.
            continue;
          }
          // Match!
          if (aCreate) {
            // In a READ-WRITE transaction, append one more possible address for
            // this participant record.
            participantRecord.addresses =
              participantRecord.addresses.concat(allPossibleAddresses);
            cursor.update(participantRecord);
          }

          if (DEBUG) {
            debug("findParticipantRecordByAddress: match "
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
    if (DEBUG) debug("Going to store " + JSON.stringify(aMessageRecord));

    let self = this;
    this.newTxn(READ_WRITE, function(error, txn, stores) {
      let notifyResult = function(rv) {
        if (aCallback) {
          aCallback.notify(rv, self.createDomMessageFromRecord(aMessageRecord));
        }
      };

      if (error) {
        // TODO bug 832140 check event.target.errorCode
        notifyResult(Cr.NS_ERROR_FAILURE);
        return;
      }

      txn.oncomplete = function oncomplete(event) {
        if (aMessageRecord.id > self.lastMessageId) {
          self.lastMessageId = aMessageRecord.id;
        }
        notifyResult(Cr.NS_OK);
      };
      txn.onabort = function onabort(event) {
        // TODO bug 832140 check event.target.errorCode
        notifyResult(Cr.NS_ERROR_FAILURE);
      };

      let messageStore = stores[0];
      let participantStore = stores[1];
      let threadStore = stores[2];
      self.replaceShortMessageOnSave(txn, messageStore, participantStore,
                                     threadStore, aMessageRecord, aAddresses);
    }, [MESSAGE_STORE_NAME, PARTICIPANT_STORE_NAME, THREAD_STORE_NAME]);
  },

  replaceShortMessageOnSave:
    function replaceShortMessageOnSave(aTransaction, aMessageStore,
                                       aParticipantStore, aThreadStore,
                                       aMessageRecord, aAddresses) {
    if (aMessageRecord.type != "sms" ||
        aMessageRecord.delivery != DELIVERY_RECEIVED ||
        !(aMessageRecord.pid >= RIL.PDU_PID_REPLACE_SHORT_MESSAGE_TYPE_1 &&
          aMessageRecord.pid <= RIL.PDU_PID_REPLACE_SHORT_MESSAGE_TYPE_7)) {
      this.realSaveRecord(aTransaction, aMessageStore, aParticipantStore,
                          aThreadStore, aMessageRecord, aAddresses);
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
    this.findParticipantRecordByAddress(aParticipantStore,
                                        aMessageRecord.sender, false,
                                        function(participantRecord) {
      if (!participantRecord) {
        self.realSaveRecord(aTransaction, aMessageStore, aParticipantStore,
                            aThreadStore, aMessageRecord, aAddresses);
        return;
      }

      let participantId = participantRecord.id;
      let range = IDBKeyRange.bound([participantId, 0], [participantId, ""]);
      let request = aMessageStore.index("participantIds").openCursor(range);
      request.onsuccess = function onsuccess(event) {
        let cursor = event.target.result;
        if (!cursor) {
          self.realSaveRecord(aTransaction, aMessageStore, aParticipantStore,
                              aThreadStore, aMessageRecord, aAddresses);
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
        self.realSaveRecord(aTransaction, aMessageStore, aParticipantStore,
                            aThreadStore, aMessageRecord, aAddresses);
      };
    });
  },

  realSaveRecord: function realSaveRecord(aTransaction, aMessageStore,
                                          aParticipantStore, aThreadStore,
                                          aMessageRecord, aAddresses) {
    let self = this;
    this.findThreadRecordByParticipants(aThreadStore, aParticipantStore,
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
        for each (let id in participantIds) {
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
                                             aMessageRecord.id,
                                             oldMessageRecord.read);
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

  forEachMatchedMmsDeliveryInfo:
    function forEachMatchedMmsDeliveryInfo(aDeliveryInfo, aNeedle, aCallback) {

    let typedAddress = MMS.Address.resolveType(aNeedle);
    let normalizedAddress, parsedAddress;
    if (typedAddress.type === "PLMN") {
      normalizedAddress = PhoneNumberUtils.normalize(aNeedle, false);
      parsedAddress = PhoneNumberUtils.parse(normalizedAddress);
    }

    for (let element of aDeliveryInfo) {
      let typedStoredAddress = MMS.Address.resolveType(element.receiver);
      if (typedAddress.type !== typedStoredAddress.type) {
        // Not even my type.  Skip.
        continue;
      }

      if (typedAddress.address == typedStoredAddress.address) {
        // Have a direct match.
        aCallback(element);
        continue;
      }

      if (typedAddress.type !== "PLMN") {
        // Address type other than "PLMN" must have direct match.  Or, skip.
        continue;
      }

      // Both are of "PLMN" type.
      let normalizedStoredAddress =
        PhoneNumberUtils.normalize(element.receiver, false);
      let parsedStoredAddress =
        PhoneNumberUtils.parseWithMCC(normalizedStoredAddress, null);
      if (this.matchPhoneNumbers(normalizedAddress, parsedAddress,
                                 normalizedStoredAddress, parsedStoredAddress)) {
        aCallback(element);
      }
    }
  },

  updateMessageDeliveryById: function updateMessageDeliveryById(
      id, type, receiver, delivery, deliveryStatus, envelopeId, callback) {
    if (DEBUG) {
      debug("Setting message's delivery by " + type + " = "+ id
            + " receiver: " + receiver
            + " delivery: " + delivery
            + " deliveryStatus: " + deliveryStatus
            + " envelopeId: " + envelopeId);
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

      let getRequest;
      if (type === "messageId") {
        getRequest = messageStore.get(id);
      } else if (type === "envelopeId") {
        getRequest = messageStore.index("envelopeId").get(id);
      }

      getRequest.onsuccess = function onsuccess(event) {
        messageRecord = event.target.result;
        if (!messageRecord) {
          if (DEBUG) debug("type = " + id + " is not found");
          return;
        }

        let isRecordUpdated = false;

        // Update |messageRecord.delivery| if needed.
        if (delivery && messageRecord.delivery != delivery) {
          messageRecord.delivery = delivery;
          messageRecord.deliveryIndex = [delivery, messageRecord.timestamp];
          isRecordUpdated = true;
        }

        // Attempt to update |deliveryStatus| and |deliveryTimestamp| of:
        // - the |messageRecord| for SMS.
        // - the element(s) in |messageRecord.deliveryInfo| for MMS.
        if (deliveryStatus) {
          // A callback for updating the deliveyStatus/deliveryTimestamp of
          // each target.
          let updateFunc = function(aTarget) {
            if (aTarget.deliveryStatus == deliveryStatus) {
              return;
            }

            aTarget.deliveryStatus = deliveryStatus;

            // Update |deliveryTimestamp| if it's successfully delivered.
            if (deliveryStatus == DELIVERY_STATUS_SUCCESS) {
              aTarget.deliveryTimestamp = Date.now();
            }

            isRecordUpdated = true;
          };

          if (messageRecord.type == "sms") {
            updateFunc(messageRecord);
          } else if (messageRecord.type == "mms") {
            if (!receiver) {
              // If the receiver is specified, we only need to update the
              // element(s) in deliveryInfo that match the same receiver.
              messageRecord.deliveryInfo.forEach(updateFunc);
            } else {
              self.forEachMatchedMmsDeliveryInfo(messageRecord.deliveryInfo,
                                                 receiver, updateFunc);
            }
          }
        }

        // Update |messageRecord.envelopeIdIndex| if needed.
        if (envelopeId) {
          if (messageRecord.envelopeIdIndex != envelopeId) {
            messageRecord.envelopeIdIndex = envelopeId;
            isRecordUpdated = true;
          }
        }

        if (!isRecordUpdated) {
          if (DEBUG) {
            debug("The values of delivery, deliveryStatus and envelopeId " +
                  "don't need to be updated.");
          }
          return;
        }

        if (DEBUG) {
          debug("The delivery, deliveryStatus or envelopeId are updated.");
        }
        messageStore.put(messageRecord);
      };
    });
  },

  fillReceivedMmsThreadParticipants: function fillReceivedMmsThreadParticipants(aMessage, threadParticipants) {
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

  updateThreadByMessageChange: function updateThreadByMessageChange(messageStore,
                                                                    threadStore,
                                                                    threadId,
                                                                    messageId,
                                                                    messageRead) {
    threadStore.get(threadId).onsuccess = function(event) {
      // This must exist.
      let threadRecord = event.target.result;
      if (DEBUG) debug("Updating thread record " + JSON.stringify(threadRecord));

      if (!messageRead) {
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
          let lastMessageSubject;
          if (nextMsg.type == "mms") {
            lastMessageSubject = nextMsg.headers.subject;
          }
          threadRecord.lastMessageSubject = lastMessageSubject || null;
          threadRecord.lastMessageId = nextMsg.id;
          threadRecord.lastTimestamp = nextMsg.timestamp;
          threadRecord.body = nextMsg.body;
          threadRecord.lastMessageType = nextMsg.type;
          if (DEBUG) {
            debug("Updating mru entry: " +
                  JSON.stringify(threadRecord));
          }
          threadStore.put(threadRecord);
        };
      } else if (!messageRead) {
        // Shortcut, just update the unread count.
        if (DEBUG) {
          debug("Updating unread count for thread id " + threadId + ": " +
                (threadRecord.unreadCount + 1) + " -> " +
                threadRecord.unreadCount);
        }
        threadStore.put(threadRecord);
      }
    };
  },

  /**
   * nsIRilMobileMessageDatabaseService API
   */

  saveReceivedMessage: function saveReceivedMessage(aMessage, aCallback) {
    if ((aMessage.type != "sms" && aMessage.type != "mms") ||
        (aMessage.type == "sms" && aMessage.messageClass == undefined) ||
        (aMessage.type == "mms" && (aMessage.delivery == undefined ||
                                    aMessage.transactionId == undefined ||
                                    !Array.isArray(aMessage.deliveryInfo) ||
                                    !Array.isArray(aMessage.receivers))) ||
        aMessage.sender == undefined ||
        aMessage.timestamp == undefined) {
      if (aCallback) {
        aCallback.notify(Cr.NS_ERROR_FAILURE, null);
      }
      return;
    }
    let threadParticipants = [aMessage.sender];
    if (aMessage.type == "mms") {
      this.fillReceivedMmsThreadParticipants(aMessage, threadParticipants);
    }

    let timestamp = aMessage.timestamp;

    // Adding needed indexes and extra attributes for internal use.
    // threadIdIndex & participantIdsIndex are filled in saveRecord().
    aMessage.readIndex = [FILTER_READ_UNREAD, timestamp];
    aMessage.read = FILTER_READ_UNREAD;

    if (aMessage.type == "mms") {
      aMessage.transactionIdIndex = aMessage.transactionId;
      aMessage.isReadReportSent = false;

      // If |deliveryTimestamp| is not specified, use 0 as default.
      let deliveryInfo = aMessage.deliveryInfo;
      for (let i = 0; i < deliveryInfo.length; i++) {
        if (deliveryInfo[i].deliveryTimestamp == undefined) {
          deliveryInfo[i].deliveryTimestamp = 0;
        }
      }
    }

    if (aMessage.type == "sms") {
      aMessage.delivery = DELIVERY_RECEIVED;
      aMessage.deliveryStatus = DELIVERY_STATUS_SUCCESS;

      if (aMessage.pid == undefined) {
        aMessage.pid = RIL.PDU_PID_DEFAULT;
      }

      // If |deliveryTimestamp| is not specified, use 0 as default.
      if (aMessage.deliveryTimestamp == undefined) {
        aMessage.deliveryTimestamp = 0;
      }
    }
    aMessage.deliveryIndex = [aMessage.delivery, timestamp];

    this.saveRecord(aMessage, threadParticipants, aCallback);
  },

  saveSendingMessage: function saveSendingMessage(aMessage, aCallback) {
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
      aMessage.deliveryInfo = [];
      for (let i = 0; i < receivers.length; i++) {
        aMessage.deliveryInfo.push({
          receiver: receivers[i],
          deliveryStatus: deliveryStatus,
          deliveryTimestamp: 0 });
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

    let addresses;
    if (aMessage.type == "sms") {
      addresses = [aMessage.receiver];
    } else if (aMessage.type == "mms") {
      addresses = aMessage.receivers;
    }
    this.saveRecord(aMessage, addresses, aCallback);
  },

  setMessageDeliveryByMessageId: function setMessageDeliveryByMessageId(
      messageId, receiver, delivery, deliveryStatus, envelopeId, callback) {
    this.updateMessageDeliveryById(messageId, "messageId",
                                   receiver, delivery, deliveryStatus,
                                   envelopeId, callback);

  },

  setMessageDeliveryByEnvelopeId: function setMessageDeliveryByEnvelopeId(
      envelopeId, receiver, delivery, deliveryStatus, callback) {
    this.updateMessageDeliveryById(envelopeId, "envelopeId",
                                   receiver, delivery, deliveryStatus,
                                   null, callback);

  },

  getMessageRecordByTransactionId: function getMessageRecordByTransactionId(aTransactionId, aCallback) {
    if (DEBUG) debug("Retrieving message with transaction ID " + aTransactionId);
    let self = this;
    this.newTxn(READ_ONLY, function (error, txn, messageStore) {
      if (error) {
        if (DEBUG) debug(error);
        aCallback.notify(Ci.nsIMobileMessageCallback.INTERNAL_ERROR, null, null);
        return;
      }
      let request = messageStore.index("transactionId").get(aTransactionId);

      txn.oncomplete = function oncomplete(event) {
        if (DEBUG) debug("Transaction " + txn + " completed.");
        let messageRecord = request.result;
        if (!messageRecord) {
          if (DEBUG) debug("Transaction ID " + aTransactionId + " not found");
          aCallback.notify(Ci.nsIMobileMessageCallback.NOT_FOUND_ERROR, null, null);
          return;
        }
        // In this case, we don't need a dom message. Just pass null to the
        // third argument.
        aCallback.notify(Ci.nsIMobileMessageCallback.SUCCESS_NO_ERROR,
                         messageRecord, null);
      };

      txn.onerror = function onerror(event) {
        if (DEBUG) {
          if (event.target) {
            debug("Caught error on transaction", event.target.errorCode);
          }
        }
        aCallback.notify(Ci.nsIMobileMessageCallback.INTERNAL_ERROR, null, null);
      };
    });
  },

  getMessageRecordById: function getMessageRecordById(aMessageId, aCallback) {
    if (DEBUG) debug("Retrieving message with ID " + aMessageId);
    let self = this;
    this.newTxn(READ_ONLY, function (error, txn, messageStore) {
      if (error) {
        if (DEBUG) debug(error);
        aCallback.notify(Ci.nsIMobileMessageCallback.INTERNAL_ERROR, null, null);
        return;
      }
      let request = messageStore.mozGetAll(aMessageId);

      txn.oncomplete = function oncomplete() {
        if (DEBUG) debug("Transaction " + txn + " completed.");
        if (request.result.length > 1) {
          if (DEBUG) debug("Got too many results for id " + aMessageId);
          aCallback.notify(Ci.nsIMobileMessageCallback.UNKNOWN_ERROR, null, null);
          return;
        }
        let messageRecord = request.result[0];
        if (!messageRecord) {
          if (DEBUG) debug("Message ID " + aMessageId + " not found");
          aCallback.notify(Ci.nsIMobileMessageCallback.NOT_FOUND_ERROR, null, null);
          return;
        }
        if (messageRecord.id != aMessageId) {
          if (DEBUG) {
            debug("Requested message ID (" + aMessageId + ") is " +
                  "different from the one we got");
          }
          aCallback.notify(Ci.nsIMobileMessageCallback.UNKNOWN_ERROR, null, null);
          return;
        }
        let domMessage = self.createDomMessageFromRecord(messageRecord);
        aCallback.notify(Ci.nsIMobileMessageCallback.SUCCESS_NO_ERROR,
                         messageRecord, domMessage);
      };

      txn.onerror = function onerror(event) {
        if (DEBUG) {
          if (event.target) {
            debug("Caught error on transaction", event.target.errorCode);
          }
        }
        aCallback.notify(Ci.nsIMobileMessageCallback.INTERNAL_ERROR, null, null);
      };
    });
  },

  /**
   * nsIMobileMessageDatabaseService API
   */

  getMessage: function getMessage(aMessageId, aRequest) {
    if (DEBUG) debug("Retrieving message with ID " + aMessageId);
    let notifyCallback = {
      notify: function notify(aRv, aMessageRecord, aDomMessage) {
        if (Ci.nsIMobileMessageCallback.SUCCESS_NO_ERROR == aRv) {
          aRequest.notifyMessageGot(aDomMessage);
          return;
        }
        aRequest.notifyGetMessageFailed(aRv, null);
      }
    };
    this.getMessageRecordById(aMessageId, notifyCallback);
  },

  deleteMessage: function deleteMessage(messageIds, length, aRequest) {
    if (DEBUG) debug("deleteMessage: message ids " + JSON.stringify(messageIds));
    let deleted = [];
    let self = this;
    this.newTxn(READ_WRITE, function (error, txn, stores) {
      if (error) {
        if (DEBUG) debug("deleteMessage: failed to open transaction");
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

      txn.oncomplete = function oncomplete(event) {
        if (DEBUG) debug("Transaction " + txn + " completed.");
        aRequest.notifyMessageDeleted(deleted, length);
      };

      for (let i = 0; i < length; i++) {
        let messageId = messageIds[i];
        deleted[i] = false;
        messageStore.get(messageId).onsuccess = function(messageIndex, event) {
          let messageRecord = event.target.result;
          let messageId = messageIds[messageIndex];
          if (messageRecord) {
            if (DEBUG) debug("Deleting message id " + messageId);

            // First actually delete the message.
            messageStore.delete(messageId).onsuccess = function(event) {
              if (DEBUG) debug("Message id " + messageId + " deleted");
              deleted[messageIndex] = true;

              // Then update unread count and most recent message.
              self.updateThreadByMessageChange(messageStore,
                                               threadStore,
                                               messageRecord.threadId,
                                               messageId,
                                               messageRecord.read);

              Services.obs.notifyObservers(null,
                                           "mobile-message-deleted",
                                           JSON.stringify({ id: messageId }));
            };
          } else if (DEBUG) {
            debug("Message id " + messageId + " does not exist");
          }
        }.bind(null, i);
      }
    }, [MESSAGE_STORE_NAME, THREAD_STORE_NAME]);
  },

  createMessageCursor: function createMessageCursor(filter, reverse, callback) {
    if (DEBUG) {
      debug("Creating a message cursor. Filters:" +
            " startDate: " + filter.startDate +
            " endDate: " + filter.endDate +
            " delivery: " + filter.delivery +
            " numbers: " + filter.numbers +
            " read: " + filter.read +
            " threadId: " + filter.threadId +
            " reverse: " + reverse);
    }

    let cursor = new GetMessagesCursor(this, callback);

    let self = this;
    self.newTxn(READ_ONLY, function (error, txn, stores) {
      let collector = cursor.collector;
      let collect = collector.collect.bind(collector);
      FilterSearcherHelper.transact(self, txn, error, filter, reverse, collect);
    }, [MESSAGE_STORE_NAME, PARTICIPANT_STORE_NAME]);

    return cursor;
  },

  markMessageRead: function markMessageRead(messageId, value, aSendReadReport, aRequest) {
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
        let readReportMessageId, readReportTo;
        if (aSendReadReport &&
            messageRecord.type == "mms" &&
            messageRecord.delivery == DELIVERY_RECEIVED &&
            messageRecord.read == FILTER_READ_READ &&
            !messageRecord.isReadReportSent) {
          messageRecord.isReadReportSent = true;

          let from = messageRecord.headers["from"];
          readReportTo = from && from.address;
          readReportMessageId = messageRecord.headers["message-id"];
        }

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
              if(readReportMessageId && readReportTo) {
                gMMSService.sendReadReport(readReportMessageId,
                                           readReportTo,
                                           messageRecord.iccId);
              }
              aRequest.notifyMessageMarkedRead(messageRecord.read);
            };
          };
        };
      };
    }, [MESSAGE_STORE_NAME, THREAD_STORE_NAME]);
  },

  createThreadCursor: function createThreadCursor(callback) {
    if (DEBUG) debug("Getting thread list");

    let cursor = new GetThreadsCursor(this, callback);
    this.newTxn(READ_ONLY, function (error, txn, threadStore) {
      let collector = cursor.collector;
      if (error) {
        if (DEBUG) debug(error);
        collector.collect(null, COLLECT_ID_ERROR, COLLECT_TIMESTAMP_UNUSED);
        return;
      }
      txn.onerror = function onerror(event) {
        if (DEBUG) debug("Caught error on transaction ", event.target.errorCode);
        collector.collect(null, COLLECT_ID_ERROR, COLLECT_TIMESTAMP_UNUSED);
      };
      let request = threadStore.index("lastTimestamp").openKeyCursor();
      request.onsuccess = function(event) {
        let cursor = event.target.result;
        if (cursor) {
          if (collector.collect(txn, cursor.primaryKey, cursor.key)) {
            cursor.continue();
          }
        } else {
          collector.collect(txn, COLLECT_ID_END, COLLECT_TIMESTAMP_UNUSED);
        }
      };
    }, [THREAD_STORE_NAME]);

    return cursor;
  }
};

let FilterSearcherHelper = {

  /**
   * @param index
   *        The name of a message store index to filter on.
   * @param range
   *        A IDBKeyRange.
   * @param direction
   *        NEXT or PREV.
   * @param txn
   *        Ongoing IDBTransaction context object.
   * @param collect
   *        Result colletor function. It takes three parameters -- txn, message
   *        id, and message timestamp.
   */
  filterIndex: function filterIndex(index, range, direction, txn, collect) {
    let messageStore = txn.objectStore(MESSAGE_STORE_NAME);
    let request = messageStore.index(index).openKeyCursor(range, direction);
    request.onsuccess = function onsuccess(event) {
      let cursor = event.target.result;
      // Once the cursor has retrieved all keys that matches its key range,
      // the filter search is done.
      if (cursor) {
        let timestamp = Array.isArray(cursor.key) ? cursor.key[1] : cursor.key;
        if (collect(txn, cursor.primaryKey, timestamp)) {
          cursor.continue();
        }
      } else {
        collect(txn, COLLECT_ID_END, COLLECT_TIMESTAMP_UNUSED);
      }
    };
    request.onerror = function onerror(event) {
      if (DEBUG && event) debug("IDBRequest error " + event.target.errorCode);
      collect(txn, COLLECT_ID_ERROR, COLLECT_TIMESTAMP_UNUSED);
    };
  },

  /**
   * Explicitly fiter message on the timestamp index.
   *
   * @param startDate
   *        Timestamp of the starting date.
   * @param endDate
   *        Timestamp of the ending date.
   * @param direction
   *        NEXT or PREV.
   * @param txn
   *        Ongoing IDBTransaction context object.
   * @param collect
   *        Result colletor function. It takes three parameters -- txn, message
   *        id, and message timestamp.
   */
  filterTimestamp: function filterTimestamp(startDate, endDate, direction, txn,
                                            collect) {
    let range = null;
    if (startDate != null && endDate != null) {
      range = IDBKeyRange.bound(startDate.getTime(), endDate.getTime());
    } else if (startDate != null) {
      range = IDBKeyRange.lowerBound(startDate.getTime());
    } else if (endDate != null) {
      range = IDBKeyRange.upperBound(endDate.getTime());
    }
    this.filterIndex("timestamp", range, direction, txn, collect);
  },

  /**
   * Instanciate a filtering transaction.
   *
   * @param service
   *        A MobileMessageDatabaseService. Used to create
   * @param txn
   *        Ongoing IDBTransaction context object.
   * @param error
   *        Previous error while creating the transaction.
   * @param filter
   *        A SmsFilter object.
   * @param reverse
   *        A boolean value indicating whether we should filter message in
   *        reversed order.
   * @param collect
   *        Result colletor function. It takes three parameters -- txn, message
   *        id, and message timestamp.
   */
  transact: function transact(service, txn, error, filter, reverse, collect) {
    if (error) {
      //TODO look at event.target.errorCode, pick appropriate error constant.
      if (DEBUG) debug("IDBRequest error " + error.target.errorCode);
      collect(txn, COLLECT_ID_ERROR, COLLECT_TIMESTAMP_UNUSED);
      return;
    }

    let direction = reverse ? PREV : NEXT;

    // We support filtering by date range only (see `else` block below) or by
    // number/delivery status/read status with an optional date range.
    if (filter.delivery == null &&
        filter.numbers == null &&
        filter.read == null &&
        filter.threadId == null) {
      // Filtering by date range only.
      if (DEBUG) {
        debug("filter.timestamp " + filter.startDate + ", " + filter.endDate);
      }

      this.filterTimestamp(filter.startDate, filter.endDate, direction, txn,
                           collect);
      return;
    }

    // Numeric 0 is smaller than any time stamp, and empty string is larger
    // than all numeric values.
    let startDate = 0, endDate = "";
    if (filter.startDate != null) {
      startDate = filter.startDate.getTime();
    }
    if (filter.endDate != null) {
      endDate = filter.endDate.getTime();
    }

    let single, intersectionCollector;
    {
      let num = 0;
      if (filter.delivery) num++;
      if (filter.numbers) num++;
      if (filter.read != undefined) num++;
      if (filter.threadId != undefined) num++;
      single = (num == 1);
    }

    if (!single) {
      intersectionCollector = new IntersectionResultsCollector(collect, reverse);
    }

    // Retrieve the keys from the 'delivery' index that matches the value of
    // filter.delivery.
    if (filter.delivery) {
      if (DEBUG) debug("filter.delivery " + filter.delivery);
      let delivery = filter.delivery;
      let range = IDBKeyRange.bound([delivery, startDate], [delivery, endDate]);
      this.filterIndex("delivery", range, direction, txn,
                       single ? collect : intersectionCollector.newContext());
    }

    // Retrieve the keys from the 'read' index that matches the value of
    // filter.read.
    if (filter.read != undefined) {
      if (DEBUG) debug("filter.read " + filter.read);
      let read = filter.read ? FILTER_READ_READ : FILTER_READ_UNREAD;
      let range = IDBKeyRange.bound([read, startDate], [read, endDate]);
      this.filterIndex("read", range, direction, txn,
                       single ? collect : intersectionCollector.newContext());
    }

    // Retrieve the keys from the 'threadId' index that matches the value of
    // filter.threadId.
    if (filter.threadId != undefined) {
      if (DEBUG) debug("filter.threadId " + filter.threadId);
      let threadId = filter.threadId;
      let range = IDBKeyRange.bound([threadId, startDate], [threadId, endDate]);
      this.filterIndex("threadId", range, direction, txn,
                       single ? collect : intersectionCollector.newContext());
    }

    // Retrieve the keys from the 'sender' and 'receiver' indexes that
    // match the values of filter.numbers
    if (filter.numbers) {
      if (DEBUG) debug("filter.numbers " + filter.numbers.join(", "));

      if (!single) {
        collect = intersectionCollector.newContext();
      }

      let participantStore = txn.objectStore(PARTICIPANT_STORE_NAME);
      service.findParticipantIdsByAddresses(participantStore, filter.numbers,
                                            false, true,
                                            (function (participantIds) {
        if (!participantIds || !participantIds.length) {
          // Oops! No such participant at all.

          collect(txn, COLLECT_ID_END, COLLECT_TIMESTAMP_UNUSED);
          return;
        }

        if (participantIds.length == 1) {
          let id = participantIds[0];
          let range = IDBKeyRange.bound([id, startDate], [id, endDate]);
          this.filterIndex("participantIds", range, direction, txn, collect);
          return;
        }

        let unionCollector = new UnionResultsCollector(collect);

        this.filterTimestamp(filter.startDate, filter.endDate, direction, txn,
                             unionCollector.newTimestampContext());

        for (let i = 0; i < participantIds.length; i++) {
          let id = participantIds[i];
          let range = IDBKeyRange.bound([id, startDate], [id, endDate]);
          this.filterIndex("participantIds", range, direction, txn,
                           unionCollector.newContext());
        }
      }).bind(this));
    }
  }
};

function ResultsCollector() {
  this.results = [];
  this.done = false;
}
ResultsCollector.prototype = {
  results: null,
  requestWaiting: null,
  done: null,

  /**
   * Queue up passed id, reply if necessary.
   *
   * @param txn
   *        Ongoing IDBTransaction context object.
   * @param id
   *        COLLECT_ID_END(0) for no more results, COLLECT_ID_ERROR(-1) for
   *        errors and valid otherwise.
   * @param timestamp
   *        We assume this function is always called in timestamp order. So
   *        this parameter is actually unused.
   *
   * @return true if expects more. false otherwise.
   */
  collect: function collect(txn, id, timestamp) {
    if (this.done) {
      return false;
    }

    if (DEBUG) {
      debug("collect: message ID = " + id);
    }
    if (id) {
      // Queue up any id but '0' and replies later accordingly.
      this.results.push(id);
    }
    if (id <= 0) {
      // No more processing on '0' or negative values passed.
      this.done = true;
    }

    if (!this.requestWaiting) {
      if (DEBUG) debug("Cursor.continue() not called yet");
      return !this.done;
    }

    // We assume there is only one request waiting throughout the message list
    // retrieving process. So we don't bother continuing to process further
    // waiting requests here. This assumption comes from DOMCursor::Continue()
    // implementation.
    let callback = this.requestWaiting;
    this.requestWaiting = null;

    this.drip(txn, callback);

    return !this.done;
  },

  /**
   * Callback right away with the first queued result entry if the filtering is
   * done. Or queue up the request and callback when a new entry is available.
   *
   * @param callback
   *        A callback function that accepts a numeric id.
   */
  squeeze: function squeeze(callback) {
    if (this.requestWaiting) {
      throw new Error("Already waiting for another request!");
    }

    if (!this.done) {
      // Database transaction ongoing, let it reply for us so that we won't get
      // blocked by the existing transaction.
      this.requestWaiting = callback;
      return;
    }

    this.drip(null, callback);
  },

  /**
   * @param txn
   *        Ongoing IDBTransaction context object or null.
   * @param callback
   *        A callback function that accepts a numeric id.
   */
  drip: function drip(txn, callback) {
    if (!this.results.length) {
      if (DEBUG) debug("No messages matching the filter criteria");
      callback(txn, COLLECT_ID_END);
      return;
    }

    if (this.results[0] < 0) {
      // An previous error found. Keep the answer in results so that we can
      // reply INTERNAL_ERROR for further requests.
      if (DEBUG) debug("An previous error found");
      callback(txn, COLLECT_ID_ERROR);
      return;
    }

    let firstMessageId = this.results.shift();
    callback(txn, firstMessageId);
  }
};

function IntersectionResultsCollector(collect, reverse) {
  this.cascadedCollect = collect;
  this.reverse = reverse;
  this.contexts = [];
}
IntersectionResultsCollector.prototype = {
  cascadedCollect: null,
  reverse: false,
  contexts: null,

  /**
   * Queue up {id, timestamp} pairs, find out intersections and report to
   * |cascadedCollect|. Return true if it is still possible to have another match.
   */
  collect: function collect(contextIndex, txn, id, timestamp) {
    if (DEBUG) {
      debug("IntersectionResultsCollector: "
            + contextIndex + ", " + id + ", " + timestamp);
    }

    let contexts = this.contexts;
    let context = contexts[contextIndex];

    if (id < 0) {
      // Act as no more matched records.
      id = 0;
    }
    if (!id) {
      context.done = true;

      if (!context.results.length) {
        // Already empty, can't have further intersection results.
        return this.cascadedCollect(txn, COLLECT_ID_END, COLLECT_TIMESTAMP_UNUSED);
      }

      for (let i = 0; i < contexts.length; i++) {
        if (!contexts[i].done) {
          // Don't call |this.cascadedCollect| because |context.results| might not
          // be empty, so other contexts might still have a chance here.
          return false;
        }
      }

      // It was the last processing context and is no more processing.
      return this.cascadedCollect(txn, COLLECT_ID_END, COLLECT_TIMESTAMP_UNUSED);
    }

    // Search id in other existing results. If no other results has it,
    // and A) the last timestamp is smaller-equal to current timestamp,
    // we wait for further results; either B) record timestamp is larger
    // then current timestamp or C) no more processing for a filter, then we
    // drop this id because there can't be a match anymore.
    for (let i = 0; i < contexts.length; i++) {
      if (i == contextIndex) {
        continue;
      }

      let ctx = contexts[i];
      let results = ctx.results;
      let found = false;
      for (let j = 0; j < results.length; j++) {
        let result = results[j];
        if (result.id == id) {
          found = true;
          break;
        }
        if ((!this.reverse && (result.timestamp > timestamp)) ||
            (this.reverse && (result.timestamp < timestamp))) {
          // B) Cannot find a match anymore. Drop.
          return true;
        }
      }

      if (!found) {
        if (ctx.done) {
          // C) Cannot find a match anymore. Drop.
          if (results.length) {
            let lastResult = results[results.length - 1];
            if ((!this.reverse && (lastResult.timestamp >= timestamp)) ||
                (this.reverse && (lastResult.timestamp <= timestamp))) {
              // Still have a chance to get another match. Return true.
              return true;
            }
          }

          // Impossible to find another match because all results in ctx have
          // timestamps smaller than timestamp.
          context.done = true;
          return this.cascadedCollect(txn, COLLECT_ID_END, COLLECT_TIMESTAMP_UNUSED);
        }

        // A) Pending.
        context.results.push({
          id: id,
          timestamp: timestamp
        });
        return true;
      }
    }

    // Now id is found in all other results. Report it.
    return this.cascadedCollect(txn, id, timestamp);
  },

  newContext: function newContext() {
    let contextIndex = this.contexts.length;
    this.contexts.push({
      results: [],
      done: false
    });
    return this.collect.bind(this, contextIndex);
  }
};

function UnionResultsCollector(collect) {
  this.cascadedCollect = collect;
  this.contexts = [{
    // Timestamp.
    processing: 1,
    results: []
  }, {
    processing: 0,
    results: []
  }];
}
UnionResultsCollector.prototype = {
  cascadedCollect: null,
  contexts: null,

  collect: function collect(contextIndex, txn, id, timestamp) {
    if (DEBUG) {
      debug("UnionResultsCollector: "
            + contextIndex + ", " + id + ", " + timestamp);
    }

    let contexts = this.contexts;
    let context = contexts[contextIndex];

    if (id < 0) {
      // Act as no more matched records.
      id = 0;
    }
    if (id) {
      if (!contextIndex) {
        // Timestamp.
        context.results.push({
          id: id,
          timestamp: timestamp
        });
      } else {
        context.results.push(id);
      }
      return true;
    }

    context.processing -= 1;
    if (contexts[0].processing || contexts[1].processing) {
      // At least one queue is still processing, but we got here because
      // current cursor gives 0 as id meaning no more messages are
      // available. Return false here to stop further cursor.continue() calls.
      return false;
    }

    let tres = contexts[0].results;
    let qres = contexts[1].results;
    tres = tres.filter(function (element) {
      return qres.indexOf(element.id) != -1;
    });

    for (let i = 0; i < tres.length; i++) {
      this.cascadedCollect(txn, tres[i].id, tres[i].timestamp);
    }
    this.cascadedCollect(txn, COLLECT_ID_END, COLLECT_TIMESTAMP_UNUSED);

    return false;
  },

  newTimestampContext: function newTimestampContext() {
    return this.collect.bind(this, 0);
  },

  newContext: function newContext() {
    this.contexts[1].processing++;
    return this.collect.bind(this, 1);
  }
};

function GetMessagesCursor(service, callback) {
  this.service = service;
  this.callback = callback;
  this.collector = new ResultsCollector();

  this.handleContinue(); // Trigger first run.
}
GetMessagesCursor.prototype = {
  classID: RIL_GETMESSAGESCURSOR_CID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICursorContinueCallback]),

  service: null,
  callback: null,
  collector: null,

  getMessageTxn: function getMessageTxn(messageStore, messageId) {
    if (DEBUG) debug ("Fetching message " + messageId);

    let getRequest = messageStore.get(messageId);
    let self = this;
    getRequest.onsuccess = function onsuccess(event) {
      if (DEBUG) {
        debug("notifyNextMessageInListGot - messageId: " + messageId);
      }
      let domMessage =
        self.service.createDomMessageFromRecord(event.target.result);
      self.callback.notifyCursorResult(domMessage);
    };
    getRequest.onerror = function onerror(event) {
      if (DEBUG) {
        debug("notifyCursorError - messageId: " + messageId);
      }
      self.callback.notifyCursorError(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
    };
  },

  notify: function notify(txn, messageId) {
    if (!messageId) {
      this.callback.notifyCursorDone();
      return;
    }

    if (messageId < 0) {
      this.callback.notifyCursorError(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
      return;
    }

    // When filter transaction is not yet completed, we're called with current
    // ongoing transaction object.
    if (txn) {
      let messageStore = txn.objectStore(MESSAGE_STORE_NAME);
      this.getMessageTxn(messageStore, messageId);
      return;
    }

    // Or, we have to open another transaction ourselves.
    let self = this;
    this.service.newTxn(READ_ONLY, function (error, txn, messageStore) {
      if (error) {
        self.callback.notifyCursorError(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
        return;
      }
      self.getMessageTxn(messageStore, messageId);
    }, [MESSAGE_STORE_NAME]);
  },

  // nsICursorContinueCallback

  handleContinue: function handleContinue() {
    if (DEBUG) debug("Getting next message in list");
    this.collector.squeeze(this.notify.bind(this));
  }
};

function GetThreadsCursor(service, callback) {
  this.service = service;
  this.callback = callback;
  this.collector = new ResultsCollector();

  this.handleContinue(); // Trigger first run.
}
GetThreadsCursor.prototype = {
  classID: RIL_GETTHREADSCURSOR_CID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICursorContinueCallback]),

  service: null,
  callback: null,
  collector: null,

  getThreadTxn: function getThreadTxn(threadStore, threadId) {
    if (DEBUG) debug ("Fetching thread " + threadId);

    let getRequest = threadStore.get(threadId);
    let self = this;
    getRequest.onsuccess = function onsuccess(event) {
      let threadRecord = event.target.result;
      if (DEBUG) {
        debug("notifyCursorResult: " + JSON.stringify(threadRecord));
      }
      let thread =
        gMobileMessageService.createThread(threadRecord.id,
                                           threadRecord.participantAddresses,
                                           threadRecord.lastTimestamp,
                                           threadRecord.lastMessageSubject || "",
                                           threadRecord.body,
                                           threadRecord.unreadCount,
                                           threadRecord.lastMessageType);
      self.callback.notifyCursorResult(thread);
    };
    getRequest.onerror = function onerror(event) {
      if (DEBUG) {
        debug("notifyCursorError - threadId: " + threadId);
      }
      self.callback.notifyCursorError(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
    };
  },

  notify: function notify(txn, threadId) {
    if (!threadId) {
      this.callback.notifyCursorDone();
      return;
    }

    if (threadId < 0) {
      this.callback.notifyCursorError(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
      return;
    }

    // When filter transaction is not yet completed, we're called with current
    // ongoing transaction object.
    if (txn) {
      let threadStore = txn.objectStore(THREAD_STORE_NAME);
      this.getThreadTxn(threadStore, threadId);
      return;
    }

    // Or, we have to open another transaction ourselves.
    let self = this;
    this.service.newTxn(READ_ONLY, function (error, txn, threadStore) {
      if (error) {
        self.callback.notifyCursorError(Ci.nsIMobileMessageCallback.INTERNAL_ERROR);
        return;
      }
      self.getThreadTxn(threadStore, threadId);
    }, [THREAD_STORE_NAME]);
  },

  // nsICursorContinueCallback

  handleContinue: function handleContinue() {
    if (DEBUG) debug("Getting next thread in list");
    this.collector.squeeze(this.notify.bind(this));
  }
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MobileMessageDatabaseService]);

function debug() {
  dump("MobileMessageDatabaseService: " + Array.slice(arguments).join(" ") + "\n");
}
