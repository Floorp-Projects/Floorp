/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PhoneNumberUtils.jsm");

const RIL_SMSDATABASESERVICE_CONTRACTID = "@mozilla.org/sms/rilsmsdatabaseservice;1";
const RIL_SMSDATABASESERVICE_CID = Components.ID("{a1fa610c-eb6c-4ac2-878f-b005d5e89249}");

const DEBUG = false;
const DB_NAME = "sms";
const DB_VERSION = 6;
const STORE_NAME = "sms";
const MOST_RECENT_STORE_NAME = "most-recent";

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

XPCOMUtils.defineLazyServiceGetter(this, "gSmsService",
                                   "@mozilla.org/sms/smsservice;1",
                                   "nsISmsService");

XPCOMUtils.defineLazyServiceGetter(this, "gIDBManager",
                                   "@mozilla.org/dom/indexeddb/manager;1",
                                   "nsIIndexedDatabaseManager");

const GLOBAL_SCOPE = this;

function numberFromMessage(message) {
  return message.delivery == DELIVERY_SENT ? message.receiver : message.sender;
}

/**
 * SmsDatabaseService
 */
function SmsDatabaseService() {
  // Prime the directory service's cache to ensure that the ProfD entry exists
  // by the time IndexedDB queries for it off the main thread. (See bug 743635.)
  Services.dirsvc.get("ProfD", Ci.nsIFile);

  gIDBManager.initWindowless(GLOBAL_SCOPE);

  let that = this;
  this.newTxn(READ_ONLY, function(error, txn, store){
    if (error) {
      return;
    }
    // In order to get the highest key value, we open a key cursor in reverse
    // order and get only the first pointed value.
    let request = store.openCursor(null, PREV);
    request.onsuccess = function onsuccess(event) {
      let cursor = event.target.result;
      if (!cursor) {
        if (DEBUG) {
          debug("Could not get the last key from sms database. " +
                "Probably empty database");
        }
        return;
      }
      that.lastKey = cursor.key || 0;
      if (DEBUG) debug("Last assigned message ID was " + that.lastKey);
    };
    request.onerror = function onerror(event) {
      if (DEBUG) {
        debug("Could not get the last key from sms database " +
              event.target.errorCode);
      }
    };
  });

  this.messageLists = {};
}
SmsDatabaseService.prototype = {

  classID:   RIL_SMSDATABASESERVICE_CID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISmsDatabaseService,
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
   * Last key value stored in the database.
   */
  lastKey: 0,

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
            let objectStore = event.target.transaction.objectStore(STORE_NAME);
            self.upgradeSchema(objectStore);
            break;
          case 2:
            if (DEBUG) debug("Upgrade to version 3. Fix existing entries.")
            objectStore = event.target.transaction.objectStore(STORE_NAME);
            self.upgradeSchema2(objectStore);
            break;
          case 3:
            if (DEBUG) debug("Upgrade to version 4. Add quick threads view.")
            self.upgradeSchema3(db, event.target.transaction);
            break;
          case 4:
            if (DEBUG) debug("Upgrade to version 5. Populate quick threads view.")
            self.upgradeSchema4(event.target.transaction);
            break;
          case 5:
            if (DEBUG) debug("Upgrade to version 6. Use PhonenumberJS.")
            self.upgradeSchema5(event.target.transaction);
            break;
          default:
            event.target.transaction.abort();
            callback("Old database version: " + event.oldVersion, null);
            break;
        }
        currentVersion++;
      }
    }
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
   *        be invoked with the transaction and the 'sms' object store.
   * @param objectStores
   *        Function to call when the transaction is available. It will
   *        be invoked with the transaction and the 'sms' object store.
   */
  newTxn: function newTxn(txn_type, callback, objectStores) {
    if (!objectStores) {
      objectStores = [STORE_NAME];
    }
    if (DEBUG) debug("Opening transaction for objectStores: " + objectStores);
    this.ensureDB(function (error, db) {
      if (error) {
        if (DEBUG) debug("Could not open database: " + error);
        callback(error);
        return;
      }
      let txn = db.transaction(objectStores, txn_type);
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
      if (objectStores.length == 1) {
        if (DEBUG) debug("Retrieving object store " + objectStores[0]);
        stores = txn.objectStore(objectStores[0]);
      } else {
        stores = [];
        for each (let storeName in objectStores) {
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
    // This objectStore holds the main SMS data.
    let objectStore = db.createObjectStore(STORE_NAME, { keyPath: "id" });
    objectStore.createIndex("delivery", "delivery", { unique: false });
    objectStore.createIndex("sender", "sender", { unique: false });
    objectStore.createIndex("receiver", "receiver", { unique: false });
    objectStore.createIndex("timestamp", "timestamp", { unique: false });
    if (DEBUG) debug("Created object stores and indexes");
  },

  /**
   * Upgrade to the corresponding database schema version.
   */
  upgradeSchema: function upgradeSchema(objectStore) {
    objectStore.createIndex("read", "read", { unique: false });
  },

  upgradeSchema2: function upgradeSchema2(objectStore) {
    objectStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (!cursor) {
        return;
      }

      let message = cursor.value;
      message.messageClass = MESSAGE_CLASS_NORMAL;
      message.deliveryStatus = DELIVERY_STATUS_NOT_APPLICABLE;
      cursor.update(message);
      cursor.continue();
    }
  },

  upgradeSchema3: function upgradeSchema3(db, transaction) {
    // Delete redundant "id" index.
    let objectStore = transaction.objectStore(STORE_NAME);
    if (objectStore.indexNames.contains("id")) {
      objectStore.deleteIndex("id");
    }

    /**
     * This objectStore can be used to quickly construct a thread view of the
     * SMS database. Each entry looks like this:
     *
     * { senderOrReceiver: <String> (primary key),
     *   id: <Number>,
     *   timestamp: <Date>,
     *   body: <String>,
     *   unreadCount: <Number> }
     *
     */
    objectStore = db.createObjectStore(MOST_RECENT_STORE_NAME,
                                       { keyPath: "senderOrReceiver" });
    objectStore.createIndex("timestamp", "timestamp");
  },

  upgradeSchema4: function upgradeSchema4(transaction) {
    let threads = {};
    let smsStore = transaction.objectStore(STORE_NAME);
    let mostRecentStore = transaction.objectStore(MOST_RECENT_STORE_NAME);

    smsStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (!cursor) {
        for (let thread in threads) {
          mostRecentStore.put(threads[thread]);
        }
        return;
      }

      let message = cursor.value;
      let contact = message.sender || message.receiver;

      if (contact in threads) {
        let thread = threads[contact];
        if (!message.read) {
          thread.unreadCount++;
        }
        if (message.timestamp > thread.timestamp) {
          thread.id = message.id;
          thread.body = message.body;
          thread.timestamp = message.timestamp;
        }
      } else {
        threads[contact] = {
          senderOrReceiver: contact,
          id: message.id,
          timestamp: message.timestamp,
          body: message.body,
          unreadCount: message.read ? 0 : 1
        }
      }
      cursor.continue();
    }
  },

  upgradeSchema5: function upgradeSchema5(transaction) {
    // Don't perform any upgrade. See Bug 819560.
  },

  /**
   * Helper function to make the intersection of the partial result arrays
   * obtained within createMessageList.
   *
   * @param keys
   *        Object containing the partial result arrays.
   * @param fiter
   *        Object containing the filter search criteria used to retrieved the
   *        partial results.
   *
   * return Array of keys containing the final result of createMessageList.
   */
  keyIntersection: function keyIntersection(keys, filter) {
    // Always use keys[FILTER_TIMESTAMP] as base result set to be filtered.
    // This ensures the result set is always sorted by timestamp.
    let result = keys[FILTER_TIMESTAMP];
    if (keys[FILTER_NUMBERS].length || filter.numbers) {
      result = result.filter(function(i) {
        return keys[FILTER_NUMBERS].indexOf(i) != -1;
      });
    }
    if (keys[FILTER_DELIVERY].length || filter.delivery) {
      result = result.filter(function(i) {
        return keys[FILTER_DELIVERY].indexOf(i) != -1;
      });
    }
    if (keys[FILTER_READ].length || filter.read) {
      result = result.filter(function(i) {
        return keys[FILTER_READ].indexOf(i) != -1;
      });
    }
    return result;
  },

  /**
   * Helper function called after createMessageList gets the final result array
   * containing the list of primary keys of records that matches the provided
   * search criteria. This function retrieves from the store the message with
   * the primary key matching the first one in the message list array and keeps
   * the rest of this array in memory. It also notifies via nsISmsRequest.
   *
   * @param messageList
   *        Array of primary keys retrieved within createMessageList.
   * @param request
   *        A nsISmsRequest object.
   */
  onMessageListCreated: function onMessageListCreated(messageList, aRequest) {
    if (DEBUG) debug("Message list created: " + messageList);
    let self = this;
    self.newTxn(READ_ONLY, function (error, txn, store) {
      if (error) {
        aRequest.notifyReadMessageListFailed(Ci.nsISmsRequest.INTERNAL_ERROR);
        return;
      }

      let messageId = messageList.shift();
      if (DEBUG) debug ("Fetching message " + messageId);
      let request = store.get(messageId);
      let message;
      request.onsuccess = function (event) {
        message = request.result;
      };

      txn.oncomplete = function oncomplete(event) {
        if (DEBUG) debug("Transaction " + txn + " completed.");
        if (!message) {
          aRequest.notifyReadMessageListFailed(Ci.nsISmsRequest.INTERNAL_ERROR);
          return;
        }
        self.lastMessageListId += 1;
        self.messageLists[self.lastMessageListId] = messageList;
        let sms = gSmsService.createSmsMessage(message.id,
                                               message.delivery,
                                               message.deliveryStatus,
                                               message.sender,
                                               message.receiver,
                                               message.body,
                                               message.messageClass,
                                               message.timestamp,
                                               message.read);
        aRequest.notifyMessageListCreated(self.lastMessageListId, sms);
      };
    });
  },

  saveMessage: function saveMessage(message) {
    this.lastKey += 1;
    message.id = this.lastKey;
    if (DEBUG) debug("Going to store " + JSON.stringify(message));
    this.newTxn(READ_WRITE, function(error, txn, stores) {
      if (error) {
        return;
      }
      // First add to main objectStore.
      stores[0].put(message);

      let number = numberFromMessage(message);

      // Next update the other objectStore.
      stores[1].get(number).onsuccess = function(event) {
        let mostRecentEntry = event.target.result;
        if (mostRecentEntry) {
          let needsUpdate = false;

          if (mostRecentEntry.timestamp <= message.timestamp) {
            mostRecentEntry.timestamp = message.timestamp;
            mostRecentEntry.body = message.body;
            needsUpdate = true;
          }

          if (!message.read) {
            mostRecentEntry.unreadCount++;
            needsUpdate = true;
          }

          if (needsUpdate) {
            event.target.source.put(mostRecentEntry);
          }
        } else {
          event.target.source.add({ senderOrReceiver: number,
                                    timestamp: message.timestamp,
                                    body: message.body,
                                    id: message.id,
                                    unreadCount: message.read ? 0 : 1 });
        }
      }
    }, [STORE_NAME, MOST_RECENT_STORE_NAME]);
    // We return the key that we expect to store in the db
    return message.id;
  },


  /**
   * nsISmsDatabaseService API
   */

  saveReceivedMessage: function saveReceivedMessage(aSender, aBody, aMessageClass, aDate) {
    let receiver = this.mRIL.rilContext.icc ? this.mRIL.rilContext.icc.msisdn : null;

    // Workaround an xpconnect issue with undefined string objects.
    // See bug 808220
    if (receiver === undefined || receiver === "undefined") {
      receiver = null;
    }

    if (receiver) {
      let parsedNumber = PhoneNumberUtils.parse(receiver);
      receiver = (parsedNumber && parsedNumber.internationalNumber)
                 ? parsedNumber.internationalNumber
                 : receiver;
    }

    let sender = aSender;
    if (sender) {
      let parsedNumber = PhoneNumberUtils.parse(sender);
      sender = (parsedNumber && parsedNumber.internationalNumber)
               ? parsedNumber.internationalNumber
               : sender;
    }

    let message = {delivery:       DELIVERY_RECEIVED,
                   deliveryStatus: DELIVERY_STATUS_SUCCESS,
                   sender:         sender,
                   receiver:       receiver,
                   body:           aBody,
                   messageClass:   aMessageClass,
                   timestamp:      aDate,
                   read:           FILTER_READ_UNREAD};
    return this.saveMessage(message);
  },

  saveSentMessage: function saveSentMessage(aReceiver, aBody, aDate) {
    let sender = this.mRIL.rilContext.icc ? this.mRIL.rilContext.icc.msisdn : null;

    // Workaround an xpconnect issue with undefined string objects.
    // See bug 808220
    if (sender === undefined || sender === "undefined") {
      sender = null;
    }

    let receiver = aReceiver
    if (receiver) {
      let parsedNumber = PhoneNumberUtils.parse(receiver.toString());
      receiver = (parsedNumber && parsedNumber.internationalNumber)
                 ? parsedNumber.internationalNumber
                 : receiver;
    }

    if (sender) {
      let parsedNumber = PhoneNumberUtils.parse(sender.toString());
      sender = (parsedNumber && parsedNumber.internationalNumber)
               ? parsedNumber.internationalNumber
               : sender;
    }

    let message = {delivery:       DELIVERY_SENT,
                   deliveryStatus: DELIVERY_STATUS_PENDING,
                   sender:         sender,
                   receiver:       receiver,
                   body:           aBody,
                   messageClass:   MESSAGE_CLASS_NORMAL,
                   timestamp:      aDate,
                   read:           FILTER_READ_READ};
    return this.saveMessage(message);
  },

  setMessageDeliveryStatus: function setMessageDeliveryStatus(messageId, deliveryStatus) {
    if ((deliveryStatus != DELIVERY_STATUS_SUCCESS)
        && (deliveryStatus != DELIVERY_STATUS_ERROR)) {
      if (DEBUG) {
        debug("Setting message " + messageId + " deliveryStatus to values other"
              + " than 'success' and 'error'");
      }
      return;
    }
    if (DEBUG) {
      debug("Setting message " + messageId + " deliveryStatus to "
            + deliveryStatus);
    }
    this.newTxn(READ_WRITE, function (error, txn, store) {
      if (error) {
        if (DEBUG) debug(error);
        return;
      }

      let getRequest = store.get(messageId);
      getRequest.onsuccess = function onsuccess(event) {
        let message = event.target.result;
        if (!message) {
          if (DEBUG) debug("Message ID " + messageId + " not found");
          return;
        }
        if (message.id != messageId) {
          if (DEBUG) {
            debug("Retrieve message ID (" + messageId + ") is " +
                  "different from the one we got");
          }
          return;
        }
        // Only updates messages that are still waiting for its delivery status.
        if (message.deliveryStatus != DELIVERY_STATUS_PENDING) {
          if (DEBUG) {
            debug("The value of message.deliveryStatus is not 'pending' but "
                  + message.deliveryStatus);
          }
          return;
        }
        message.deliveryStatus = deliveryStatus;
        if (DEBUG) debug("Message.deliveryStatus set to: " + deliveryStatus);
        store.put(message);
      };
    });
  },

  getMessage: function getMessage(messageId, aRequest) {
    if (DEBUG) debug("Retrieving message with ID " + messageId);
    this.newTxn(READ_ONLY, function (error, txn, store) {
      if (error) {
        if (DEBUG) debug(error);
        aRequest.notifyGetMessageFailed(Ci.nsISmsRequest.INTERNAL_ERROR);
        return;
      }
      let request = store.mozGetAll(messageId);

      txn.oncomplete = function oncomplete() {
        if (DEBUG) debug("Transaction " + txn + " completed.");
        if (request.result.length > 1) {
          if (DEBUG) debug("Got too many results for id " + messageId);
          aRequest.notifyGetMessageFailed(Ci.nsISmsRequest.UNKNOWN_ERROR);
          return;
        }
        let data = request.result[0];
        if (!data) {
          if (DEBUG) debug("Message ID " + messageId + " not found");
          aRequest.notifyGetMessageFailed(Ci.nsISmsRequest.NOT_FOUND_ERROR);
          return;
        }
        if (data.id != messageId) {
          if (DEBUG) {
            debug("Requested message ID (" + messageId + ") is " +
                  "different from the one we got");
          }
          aRequest.notifyGetMessageFailed(Ci.nsISmsRequest.UNKNOWN_ERROR);
          return;
        }
        let message = gSmsService.createSmsMessage(data.id,
                                                   data.delivery,
                                                   data.deliveryStatus,
                                                   data.sender,
                                                   data.receiver,
                                                   data.body,
                                                   data.messageClass,
                                                   data.timestamp,
                                                   data.read);
        aRequest.notifyMessageGot(message);
      };

      txn.onerror = function onerror(event) {
        if (DEBUG) {
          if (event.target)
            debug("Caught error on transaction", event.target.errorCode);
        }
        //TODO look at event.target.errorCode, pick appropriate error constant
        aRequest.notifyGetMessageFailed(Ci.nsISmsRequest.INTERNAL_ERROR);
      };
    });
  },

  deleteMessage: function deleteMessage(messageId, aRequest) {
    let deleted = false;
    let self = this;
    this.newTxn(READ_WRITE, function (error, txn, stores) {
      if (error) {
        aRequest.notifyDeleteMessageFailed(Ci.nsISmsRequest.INTERNAL_ERROR);
        return;
      }
      txn.onerror = function onerror(event) {
        if (DEBUG) debug("Caught error on transaction", event.target.errorCode);
        //TODO look at event.target.errorCode, pick appropriate error constant
        aRequest.notifyDeleteMessageFailed(Ci.nsISmsRequest.INTERNAL_ERROR);
      };

      const smsStore = stores[0];
      const mruStore = stores[1];

      let deleted = false;

      txn.oncomplete = function oncomplete(event) {
        if (DEBUG) debug("Transaction " + txn + " completed.");
        aRequest.notifyMessageDeleted(deleted);
      };

      smsStore.get(messageId).onsuccess = function(event) {
        let message = event.target.result;
        if (message) {
          if (DEBUG) debug("Deleting message id " + messageId);

          // First actually delete the message.
          event.target.source.delete(messageId).onsuccess = function(event) {
            deleted = true;

            // Then update unread count and most recent message.
            let number = numberFromMessage(message);

            mruStore.get(number).onsuccess = function(event) {
              // This must exist.
              let mostRecentEntry = event.target.result;

              if (!message.read) {
                mostRecentEntry.unreadCount--;
              }

              if (mostRecentEntry.id == messageId) {
                // This sucks, we have to find a new most-recent message.
                message = null;

                // Check most recent sender.
                smsStore.index("sender").openCursor(number, "prev").onsuccess = function(event) {
                  let cursor = event.target.result;
                  if (cursor) {
                    message = cursor.value;
                  }
                };

                // Check most recent receiver.
                smsStore.index("receiver").openCursor(number, "prev").onsuccess = function(event) {
                  let cursor = event.target.result;
                  if (cursor) {
                    if (!message || cursor.value.timeStamp > message.timestamp) {
                      message = cursor.value;
                    }
                  }

                  // If we found a new message then we need to update the data
                  // in the most-recent store. Otherwise we can delete it.
                  if (message) {
                    mostRecentEntry.id = message.id;
                    mostRecentEntry.timestamp = message.timestamp;
                    mostRecentEntry.body = message.body;
                    if (DEBUG) {
                      debug("Updating mru entry: " +
                            JSON.stringify(mostRecentEntry));
                    }
                    mruStore.put(mostRecentEntry);
                  }
                  else {
                    if (DEBUG) {
                      debug("Deleting mru entry for number '" + number + "'");
                    }
                    mruStore.delete(number);
                  }
                };
              } else if (!message.read) {
                // Shortcut, just update the unread count.
                if (DEBUG) {
                  debug("Updating unread count for number '" + number + "': " +
                        (mostRecentEntry.unreadCount + 1) + " -> " +
                        mostRecentEntry.unreadCount);
                }
                mruStore.put(mostRecentEntry);
              }
            };
          };
        } else if (DEBUG) {
          debug("Message id " + messageId + " does not exist");
        }
      };
    }, [STORE_NAME, MOST_RECENT_STORE_NAME]);
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
    // This object keeps the lists of keys retrieved by the search specific to
    // each nsIMozSmsFilter. Once all the keys have been retrieved from the
    // store, the final intersection of this arrays will contain all the
    // keys for the message list that we are creating.
    let filteredKeys = {};
    filteredKeys[FILTER_TIMESTAMP] = [];
    filteredKeys[FILTER_NUMBERS] = [];
    filteredKeys[FILTER_DELIVERY] = [];
    filteredKeys[FILTER_READ] = [];

    // Callback function to iterate through request results via IDBCursor.
    let successCb = function onsuccess(result, filter) {
      // Once the cursor has retrieved all keys that matches its key range,
      // the filter search is done.
      if (!result) {
        if (DEBUG) {
          debug("These messages match the " + filter + " filter: " +
                filteredKeys[filter]);
        }
        return;
      }
      // The cursor primaryKey is stored in its corresponding partial array
      // according to the filter parameter.
      let primaryKey = result.primaryKey;
      filteredKeys[filter].push(primaryKey);
      result.continue();
    };

    let errorCb = function onerror(event) {
      //TODO look at event.target.errorCode, pick appropriate error constant.
      if (DEBUG) debug("IDBRequest error " + event.target.errorCode);
      aRequest.notifyReadMessageListFailed(Ci.nsISmsRequest.INTERNAL_ERROR);
      return;
    };

    let self = this;
    this.newTxn(READ_ONLY, function (error, txn, store) {
      if (error) {
        errorCb(error);
        return;
      }

      // In first place, we retrieve the keys that match the filter.startDate
      // and filter.endDate search criteria.
      let timeKeyRange = null;
      if (filter.startDate != null && filter.endDate != null) {
        timeKeyRange = IDBKeyRange.bound(filter.startDate.getTime(),
                                         filter.endDate.getTime());
      } else if (filter.startDate != null) {
        timeKeyRange = IDBKeyRange.lowerBound(filter.startDate.getTime());
      } else if (filter.endDate != null) {
        timeKeyRange = IDBKeyRange.upperBound(filter.endDate.getTime());
      }
      let direction = reverse ? PREV : NEXT;
      let timeRequest = store.index("timestamp").openKeyCursor(timeKeyRange,
                                                               direction);

      timeRequest.onsuccess = function onsuccess(event) {
        successCb(event.target.result, FILTER_TIMESTAMP);
      };
      timeRequest.onerror = errorCb;

      // Retrieve the keys from the 'delivery' index that matches the
      // value of filter.delivery.
      if (filter.delivery) {
        let deliveryKeyRange = IDBKeyRange.only(filter.delivery);
        let deliveryRequest = store.index("delivery")
                                   .openKeyCursor(deliveryKeyRange);
        deliveryRequest.onsuccess = function onsuccess(event) {
          successCb(event.target.result, FILTER_DELIVERY);
        };
        deliveryRequest.onerror = errorCb;
      }

      // Retrieve the keys from the 'sender' and 'receiver' indexes that
      // match the values of filter.numbers
      if (filter.numbers) {
        for (let i = 0; i < filter.numbers.length; i++) {
          let numberKeyRange = IDBKeyRange.only(filter.numbers[i]);
          let senderRequest = store.index("sender")
                                   .openKeyCursor(numberKeyRange);
          let receiverRequest = store.index("receiver")
                                     .openKeyCursor(numberKeyRange);
          senderRequest.onsuccess = receiverRequest.onsuccess =
            function onsuccess(event){
              successCb(event.target.result, FILTER_NUMBERS);
            };
          senderRequest.onerror = receiverRequest.onerror = errorCb;
        }
      }

      // Retrieve the keys from the 'read' index that matches the value of
      // filter.read
      if (filter.read != undefined) {
        let read = filter.read ? FILTER_READ_READ : FILTER_READ_UNREAD;
        if (DEBUG) debug("filter.read " + read);
        let readKeyRange = IDBKeyRange.only(read);
        let readRequest = store.index("read")
                               .openKeyCursor(readKeyRange);
        readRequest.onsuccess = function onsuccess(event) {
          successCb(event.target.result, FILTER_READ);
        };
        readRequest.onerror = errorCb;
      }

      txn.oncomplete = function oncomplete(event) {
        if (DEBUG) debug("Transaction " + txn + " completed.");
        // We need to get the intersection of all the partial searches to
        // get the final result array.
        let result =  self.keyIntersection(filteredKeys, filter);
        if (!result.length) {
          if (DEBUG) debug("No messages matching the filter criteria");
          aRequest.notifyNoMessageInList();
          return;
        }

        // At this point, filteredKeys should have all the keys that matches
        // all the search filters. So we take the first key and retrieve the
        // corresponding message. The rest of the keys are added to the
        // messageLists object as a new list.
        self.onMessageListCreated(result, aRequest);
      };

      txn.onerror = function onerror(event) {
        errorCb(event);
      };
    });
  },

  getNextMessageInList: function getNextMessageInList(listId, aRequest) {
    if (DEBUG) debug("Getting next message in list " + listId);
    let messageId;
    let list = this.messageLists[listId];
    if (!list) {
      if (DEBUG) debug("Wrong list id");
      aRequest.notifyReadMessageListFailed(Ci.nsISmsRequest.NOT_FOUND_ERROR);
      return;
    }
    messageId = list.shift();
    if (messageId == null) {
      if (DEBUG) debug("Reached the end of the list!");
      aRequest.notifyNoMessageInList();
      return;
    }
    this.newTxn(READ_ONLY, function (error, txn, store) {
      if (DEBUG) debug("Fetching message " + messageId);
      let request = store.get(messageId);
      let message;
      request.onsuccess = function onsuccess(event) {
        message = request.result;
      };

      txn.oncomplete = function oncomplete(event) {
        if (DEBUG) debug("Transaction " + txn + " completed.");
        if (!message) {
          if (DEBUG) debug("Could not get message id " + messageId);
          aRequest.notifyReadMessageListFailed(Ci.nsISmsRequest.NOT_FOUND_ERROR);
        }
        let sms = gSmsService.createSmsMessage(message.id,
                                               message.delivery,
                                               message.deliveryStatus,
                                               message.sender,
                                               message.receiver,
                                               message.body,
                                               message.messageClass,
                                               message.timestamp,
                                               message.read);
        aRequest.notifyNextMessageInListGot(sms);
      };

      txn.onerror = function onerror(event) {
        //TODO check event.target.errorCode
        if (DEBUG) {
          debug("Error retrieving message id: " + messageId +
                ". Error code: " + event.target.errorCode);
        }
        aRequest.notifyReadMessageListFailed(Ci.nsISmsRequest.INTERNAL_ERROR);
      };
    });
  },

  clearMessageList: function clearMessageList(listId) {
    if (DEBUG) debug("Clearing message list: " + listId);
    delete this.messageLists[listId];
  },

  markMessageRead: function markMessageRead(messageId, value, aRequest) {
    if (DEBUG) debug("Setting message " + messageId + " read to " + value);
    this.newTxn(READ_WRITE, function (error, txn, stores) {
      if (error) {
        if (DEBUG) debug(error);
        aRequest.notifyMarkMessageReadFailed(Ci.nsISmsRequest.INTERNAL_ERROR);
        return;
      }
      txn.onerror = function onerror(event) {
        if (DEBUG) debug("Caught error on transaction ", event.target.errorCode);
        aRequest.notifyMarkMessageReadFailed(Ci.nsISmsRequest.INTERNAL_ERROR);
      };
      stores[0].get(messageId).onsuccess = function onsuccess(event) {
        let message = event.target.result;
        if (!message) {
          if (DEBUG) debug("Message ID " + messageId + " not found");
          aRequest.notifyMarkMessageReadFailed(Ci.nsISmsRequest.NOT_FOUND_ERROR);
          return;
        }
        if (message.id != messageId) {
          if (DEBUG) {
            debug("Retrieve message ID (" + messageId + ") is " +
                  "different from the one we got");
          }
          aRequest.notifyMarkMessageReadFailed(Ci.nsISmsRequest.UNKNOWN_ERROR);
          return;
        }
        // If the value to be set is the same as the current message `read`
        // value, we just notify successfully.
        if (message.read == value) {
          if (DEBUG) debug("The value of message.read is already " + value);
          aRequest.notifyMessageMarkedRead(message.read);
          return;
        }
        message.read = value ? FILTER_READ_READ : FILTER_READ_UNREAD;
        if (DEBUG) debug("Message.read set to: " + value);
        event.target.source.put(message).onsuccess = function onsuccess(event) {
          if (DEBUG) {
            debug("Update successfully completed. Message: " +
                  JSON.stringify(event.target.result));
          }

          // Now update the unread count.
          let number = numberFromMessage(message);

          stores[1].get(number).onsuccess = function(event) {
            let mostRecentEntry = event.target.result;
            mostRecentEntry.unreadCount += value ? -1 : 1;
            if (DEBUG) {
              debug("Updating unreadCount for '" + number + "': " +
                    (value ?
                     mostRecentEntry.unreadCount + 1 :
                     mostRecentEntry.unreadCount - 1) +
                    " -> " + mostRecentEntry.unreadCount);
            }
            event.target.source.put(mostRecentEntry).onsuccess = function(event) {
              aRequest.notifyMessageMarkedRead(message.read);
            };
          };
        };
      };
    }, [STORE_NAME, MOST_RECENT_STORE_NAME]);
  },
  getThreadList: function getThreadList(aRequest) {
    if (DEBUG) debug("Getting thread list");
    this.newTxn(READ_ONLY, function (error, txn, store) {
      if (error) {
        if (DEBUG) debug(error);
        aRequest.notifyThreadListFailed(Ci.nsISmsRequest.INTERNAL_ERROR);
        return;
      }
      txn.onerror = function onerror(event) {
        if (DEBUG) debug("Caught error on transaction ", event.target.errorCode);
        aRequest.notifyThreadListFailed(Ci.nsISmsRequest.INTERNAL_ERROR);
      };
      store.index("timestamp").mozGetAll().onsuccess = function(event) {
        aRequest.notifyThreadList(event.target.result);
      };
    }, [MOST_RECENT_STORE_NAME]);
  }
};

XPCOMUtils.defineLazyGetter(SmsDatabaseService.prototype, "mRIL", function () {
    return Cc["@mozilla.org/telephony/system-worker-manager;1"]
              .getService(Ci.nsIInterfaceRequestor)
              .getInterface(Ci.nsIRadioInterfaceLayer);
});

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SmsDatabaseService]);

function debug() {
  dump("SmsDatabaseService: " + Array.slice(arguments).join(" ") + "\n");
}
