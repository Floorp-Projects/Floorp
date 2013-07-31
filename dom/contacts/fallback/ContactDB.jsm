/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ['ContactDB'];

const DEBUG = false;
function debug(s) { dump("-*- ContactDB component: " + s + "\n"); }

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/IndexedDBHelper.jsm");
Cu.import("resource://gre/modules/PhoneNumberUtils.jsm");

const DB_NAME = "contacts";
const DB_VERSION = 14;
const STORE_NAME = "contacts";
const SAVED_GETALL_STORE_NAME = "getallcache";
const CHUNK_SIZE = 20;
const REVISION_STORE = "revision";
const REVISION_KEY = "revision";

function exportContact(aRecord) {
  let contact = {};
  contact.properties = aRecord.properties;

  for (let field in aRecord.properties)
    contact.properties[field] = aRecord.properties[field];

  contact.updated = aRecord.updated;
  contact.published = aRecord.published;
  contact.id = aRecord.id;
  return contact;
}

function ContactDispatcher(aContacts, aFullContacts, aCallback, aNewTxn, aClearDispatcher, aFailureCb) {
  let nextIndex = 0;

  let sendChunk;
  let count = 0;
  if (aFullContacts) {
    sendChunk = function() {
      try {
        let chunk = aContacts.splice(0, CHUNK_SIZE);
        if (chunk.length > 0) {
          aCallback(chunk);
        }
        if (aContacts.length === 0) {
          aCallback(null);
          aClearDispatcher();
        }
      } catch (e) {
        aClearDispatcher();
      }
    }
  } else {
    sendChunk = function() {
      try {
        let start = nextIndex;
        nextIndex += CHUNK_SIZE;
        let chunk = [];
        aNewTxn("readonly", STORE_NAME, function(txn, store) {
          for (let i = start; i < Math.min(start+CHUNK_SIZE, aContacts.length); ++i) {
            store.get(aContacts[i]).onsuccess = function(e) {
              chunk.push(exportContact(e.target.result));
              count++;
              if (count === aContacts.length) {
                aCallback(chunk);
                aCallback(null);
                aClearDispatcher();
              } else if (chunk.length === CHUNK_SIZE) {
                aCallback(chunk);
                chunk.length = 0;
              }
            }
          }
        }, null, function(errorMsg) {
          aFailureCb(errorMsg);
        });
      } catch (e) {
        aClearDispatcher();
      }
    }
  }

  return {
    sendNow: function() {
      sendChunk();
    }
  };
}

this.ContactDB = function ContactDB(aGlobal) {
  if (DEBUG) debug("Constructor");
  this._global = aGlobal;
}

ContactDB.prototype = {
  __proto__: IndexedDBHelper.prototype,

  _dispatcher: {},

  upgradeSchema: function upgradeSchema(aTransaction, aDb, aOldVersion, aNewVersion) {
    let loadInitialContacts = function() {
      // Add default contacts
      let jsm = {};
      Cu.import("resource://gre/modules/FileUtils.jsm", jsm);
      Cu.import("resource://gre/modules/NetUtil.jsm", jsm);
      // Loading resource://app/defaults/contacts.json doesn't work because
      // contacts.json is not in the omnijar.
      // So we look for the app dir instead and go from here...
      let contactsFile = jsm.FileUtils.getFile("DefRt", ["contacts.json"], false);
      if (!contactsFile || (contactsFile && !contactsFile.exists())) {
        // For b2g desktop
        contactsFile = jsm.FileUtils.getFile("ProfD", ["contacts.json"], false);
        if (!contactsFile || (contactsFile && !contactsFile.exists())) {
          return;
        }
      }

      let chan = jsm.NetUtil.newChannel(contactsFile);
      let stream = chan.open();
      // Obtain a converter to read from a UTF-8 encoded input stream.
      let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                      .createInstance(Ci.nsIScriptableUnicodeConverter);
      converter.charset = "UTF-8";
      let rawstr = converter.ConvertToUnicode(jsm.NetUtil.readInputStreamToString(
                                              stream,
                                              stream.available()) || "");
      stream.close();
      let contacts;
      try {
        contacts = JSON.parse(rawstr);
      } catch(e) {
        if (DEBUG) debug("Error parsing " + contactsFile.path + " : " + e);
        return;
      }

      let idService = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
      objectStore = aTransaction.objectStore(STORE_NAME);

      for (let i = 0; i < contacts.length; i++) {
        let contact = {};
        contact.properties = contacts[i];
        contact.id = idService.generateUUID().toString().replace('-', '', 'g')
                                                        .replace('{', '')
                                                        .replace('}', '');
        contact = this.makeImport(contact);
        this.updateRecordMetadata(contact);
        if (DEBUG) debug("import: " + JSON.stringify(contact));
        objectStore.put(contact);
      }
    }.bind(this);

    if (DEBUG) debug("upgrade schema from: " + aOldVersion + " to " + aNewVersion + " called!");
    let db = aDb;
    let objectStore;

    let steps = [
      function upgrade0to1() {
        /**
         * Create the initial database schema.
         *
         * The schema of records stored is as follows:
         *
         * {id:            "...",       // UUID
         *  published:     Date(...),   // First published date.
         *  updated:       Date(...),   // Last updated date.
         *  properties:    {...}        // Object holding the ContactProperties
         * }
         */
        if (DEBUG) debug("create schema");
        objectStore = db.createObjectStore(STORE_NAME, {keyPath: "id"});

        // Properties indexes
        objectStore.createIndex("familyName", "properties.familyName", { multiEntry: true });
        objectStore.createIndex("givenName",  "properties.givenName",  { multiEntry: true });

        objectStore.createIndex("familyNameLowerCase", "search.familyName", { multiEntry: true });
        objectStore.createIndex("givenNameLowerCase",  "search.givenName",  { multiEntry: true });
        objectStore.createIndex("telLowerCase",        "search.tel",        { multiEntry: true });
        objectStore.createIndex("emailLowerCase",      "search.email",      { multiEntry: true });
        next();
      },
      function upgrade1to2() {
        if (DEBUG) debug("upgrade 1");

        // Create a new scheme for the tel field. We move from an array of tel-numbers to an array of
        // ContactTelephone.
        if (!objectStore) {
          objectStore = aTransaction.objectStore(STORE_NAME);
        }
        // Delete old tel index.
        if (objectStore.indexNames.contains("tel")) {
          objectStore.deleteIndex("tel");
        }

        // Upgrade existing tel field in the DB.
        objectStore.openCursor().onsuccess = function(event) {
          let cursor = event.target.result;
          if (cursor) {
            if (DEBUG) debug("upgrade tel1: " + JSON.stringify(cursor.value));
            for (let number in cursor.value.properties.tel) {
              cursor.value.properties.tel[number] = {number: number};
            }
            cursor.update(cursor.value);
            if (DEBUG) debug("upgrade tel2: " + JSON.stringify(cursor.value));
            cursor.continue();
          } else {
            next();
          }
        };

        // Create new searchable indexes.
        objectStore.createIndex("tel", "search.tel", { multiEntry: true });
        objectStore.createIndex("category", "properties.category", { multiEntry: true });
      },
      function upgrade2to3() {
        if (DEBUG) debug("upgrade 2");
        // Create a new scheme for the email field. We move from an array of emailaddresses to an array of
        // ContactEmail.
        if (!objectStore) {
          objectStore = aTransaction.objectStore(STORE_NAME);
        }

        // Delete old email index.
        if (objectStore.indexNames.contains("email")) {
          objectStore.deleteIndex("email");
        }

        // Upgrade existing email field in the DB.
        objectStore.openCursor().onsuccess = function(event) {
          let cursor = event.target.result;
          if (cursor) {
            if (cursor.value.properties.email) {
              if (DEBUG) debug("upgrade email1: " + JSON.stringify(cursor.value));
              cursor.value.properties.email =
                cursor.value.properties.email.map(function(address) { return { address: address }; });
              cursor.update(cursor.value);
              if (DEBUG) debug("upgrade email2: " + JSON.stringify(cursor.value));
            }
            cursor.continue();
          } else {
            next();
          }
        };

        // Create new searchable indexes.
        objectStore.createIndex("email", "search.email", { multiEntry: true });
      },
      function upgrade3to4() {
        if (DEBUG) debug("upgrade 3");

        if (!objectStore) {
          objectStore = aTransaction.objectStore(STORE_NAME);
        }

        // Upgrade existing impp field in the DB.
        objectStore.openCursor().onsuccess = function(event) {
          let cursor = event.target.result;
          if (cursor) {
            if (cursor.value.properties.impp) {
              if (DEBUG) debug("upgrade impp1: " + JSON.stringify(cursor.value));
              cursor.value.properties.impp =
                cursor.value.properties.impp.map(function(value) { return { value: value }; });
              cursor.update(cursor.value);
              if (DEBUG) debug("upgrade impp2: " + JSON.stringify(cursor.value));
            }
            cursor.continue();
          }
        };
        // Upgrade existing url field in the DB.
        objectStore.openCursor().onsuccess = function(event) {
          let cursor = event.target.result;
          if (cursor) {
            if (cursor.value.properties.url) {
              if (DEBUG) debug("upgrade url1: " + JSON.stringify(cursor.value));
              cursor.value.properties.url =
                cursor.value.properties.url.map(function(value) { return { value: value }; });
              cursor.update(cursor.value);
              if (DEBUG) debug("upgrade impp2: " + JSON.stringify(cursor.value));
            }
            cursor.continue();
          } else {
            next();
          }
        };
      },
      function upgrade4to5() {
        if (DEBUG) debug("Add international phone numbers upgrade");
        if (!objectStore) {
          objectStore = aTransaction.objectStore(STORE_NAME);
        }

        objectStore.openCursor().onsuccess = function(event) {
          let cursor = event.target.result;
          if (cursor) {
            if (cursor.value.properties.tel) {
              if (DEBUG) debug("upgrade : " + JSON.stringify(cursor.value));
              cursor.value.properties.tel.forEach(
                function(duple) {
                  let parsedNumber = PhoneNumberUtils.parse(duple.value.toString());
                  if (parsedNumber) {
                    if (DEBUG) {
                      debug("InternationalFormat: " + parsedNumber.internationalFormat);
                      debug("InternationalNumber: " + parsedNumber.internationalNumber);
                      debug("NationalNumber: " + parsedNumber.nationalNumber);
                      debug("NationalFormat: " + parsedNumber.nationalFormat);
                    }
                    if (duple.value.toString() !== parsedNumber.internationalNumber) {
                      cursor.value.search.tel.push(parsedNumber.internationalNumber);
                    }
                  } else {
                    dump("Warning: No international number found for " + duple.value + "\n");
                  }
                }
              )
              cursor.update(cursor.value);
            }
            if (DEBUG) debug("upgrade2 : " + JSON.stringify(cursor.value));
            cursor.continue();
          } else {
            next();
          }
        };
      },
      function upgrade5to6() {
        if (DEBUG) debug("Add index for equals tel searches");
        if (!objectStore) {
          objectStore = aTransaction.objectStore(STORE_NAME);
        }

        // Delete old tel index (not on the right field).
        if (objectStore.indexNames.contains("tel")) {
          objectStore.deleteIndex("tel");
        }

        // Create new index for "equals" searches
        objectStore.createIndex("tel", "search.exactTel", { multiEntry: true });

        objectStore.openCursor().onsuccess = function(event) {
          let cursor = event.target.result;
          if (cursor) {
            if (cursor.value.properties.tel) {
              if (DEBUG) debug("upgrade : " + JSON.stringify(cursor.value));
              cursor.value.properties.tel.forEach(
                function(duple) {
                  let number = duple.value.toString();
                  let parsedNumber = PhoneNumberUtils.parse(number);

                  cursor.value.search.exactTel = [number];
                  if (parsedNumber &&
                      parsedNumber.internationalNumber &&
                      number !== parsedNumber.internationalNumber) {
                    cursor.value.search.exactTel.push(parsedNumber.internationalNumber);
                  }
                }
              )
              cursor.update(cursor.value);
            }
            if (DEBUG) debug("upgrade : " + JSON.stringify(cursor.value));
            cursor.continue();
          } else {
            next();
          }
        };
      },
      function upgrade6to7() {
        if (!objectStore) {
          objectStore = aTransaction.objectStore(STORE_NAME);
        }
        let names = objectStore.indexNames;
        let blackList = ["tel", "familyName", "givenName",  "familyNameLowerCase",
                         "givenNameLowerCase", "telLowerCase", "category", "email",
                         "emailLowerCase"];
        for (var i = 0; i < names.length; i++) {
          if (blackList.indexOf(names[i]) < 0) {
            objectStore.deleteIndex(names[i]);
          }
        }
        next();
      },
      function upgrade7to8() {
        if (DEBUG) debug("Adding object store for cached searches");
        db.createObjectStore(SAVED_GETALL_STORE_NAME);
        next();
      },
      function upgrade8to9() {
        if (DEBUG) debug("Make exactTel only contain the value entered by the user");
        if (!objectStore) {
          objectStore = aTransaction.objectStore(STORE_NAME);
        }

        objectStore.openCursor().onsuccess = function(event) {
          let cursor = event.target.result;
          if (cursor) {
            if (cursor.value.properties.tel) {
              cursor.value.search.exactTel = [];
              cursor.value.properties.tel.forEach(
                function(tel) {
                  let normalized = PhoneNumberUtils.normalize(tel.value.toString());
                  cursor.value.search.exactTel.push(normalized);
                }
              );
              cursor.update(cursor.value);
            }
            cursor.continue();
          } else {
            next();
          }
        };
      },
      function upgrade9to10() {
        // no-op, see https://bugzilla.mozilla.org/show_bug.cgi?id=883770#c16
        next();
      },
      function upgrade10to11() {
        if (DEBUG) debug("Adding object store for database revision");
        db.createObjectStore(REVISION_STORE).put(0, REVISION_KEY);
        next();
      },
      function upgrade11to12() {
        if (DEBUG) debug("Add a telMatch index with national and international numbers");
        if (!objectStore) {
          objectStore = aTransaction.objectStore(STORE_NAME);
        }
        if (!objectStore.indexNames.contains("telMatch")) {
          objectStore.createIndex("telMatch", "search.parsedTel", {multiEntry: true});
        }
        objectStore.openCursor().onsuccess = function(event) {
          let cursor = event.target.result;
          if (cursor) {
            if (cursor.value.properties.tel) {
              cursor.value.search.parsedTel = [];
              cursor.value.properties.tel.forEach(
                function(tel) {
                  let parsed = PhoneNumberUtils.parse(tel.value.toString());
                  if (parsed) {
                    cursor.value.search.parsedTel.push(parsed.nationalNumber);
                    cursor.value.search.parsedTel.push(PhoneNumberUtils.normalize(parsed.nationalFormat));
                    cursor.value.search.parsedTel.push(parsed.internationalNumber);
                    cursor.value.search.parsedTel.push(PhoneNumberUtils.normalize(parsed.internationalFormat));
                  }
                  cursor.value.search.parsedTel.push(PhoneNumberUtils.normalize(tel.value.toString()));
                }
              );
              cursor.update(cursor.value);
            }
            cursor.continue();
          } else {
            next();
          }
        };
      },
      function upgrade12to13() {
        if (DEBUG) debug("Add phone substring to the search index if appropriate for country");
        if (this.substringMatching) {
          if (!objectStore) {
            objectStore = aTransaction.objectStore(STORE_NAME);
          }
          objectStore.openCursor().onsuccess = function(event) {
            let cursor = event.target.result;
            if (cursor) {
              if (cursor.value.properties.tel) {
                cursor.value.search.parsedTel = cursor.value.search.parsedTel || [];
                cursor.value.properties.tel.forEach(
                  function(tel) {
                    let normalized = PhoneNumberUtils.normalize(tel.value.toString());
                    if (normalized) {
                      if (this.substringMatching && normalized.length > this.substringMatching) {
                        let sub = normalized.slice(-this.substringMatching);
                        if (cursor.value.search.parsedTel.indexOf(sub) === -1) {
                          if (DEBUG) debug("Adding substring index: " + tel + ", " + sub);
                          cursor.value.search.parsedTel.push(sub);
                        }
                      }
                    }
                  }.bind(this)
                );
                cursor.update(cursor.value);
              }
              cursor.continue();
            } else {
              next();
            }
          }.bind(this);
        } else {
          next();
        }
      },
      function upgrade13to14() {
        if (DEBUG) debug("Cleaning up empty substring entries in telMatch index");
        if (!objectStore) {
          objectStore = aTransaction.objectStore(STORE_NAME);
        }
        objectStore.openCursor().onsuccess = function(event) {
          function removeEmptyStrings(value) {
            if (value) {
              const oldLength = value.length;
              for (let i = 0; i < value.length; ++i) {
                if (!value[i] || value[i] == "null") {
                  value.splice(i, 1);
                }
              }
              return oldLength !== value.length;
            }
          }

          let cursor = event.target.result;
          if (cursor) {
            let modified = removeEmptyStrings(cursor.value.search.parsedTel);
            let modified2 = removeEmptyStrings(cursor.value.search.tel);
            if (modified || modified2) {
              cursor.update(cursor.value);
            }
          } else {
            next();
          }
        };
      },
    ];

    let index = aOldVersion;
    let outer = this;
    function next() {
      if (index == aNewVersion) {
        if (aOldVersion === 0) {
          loadInitialContacts();
        }
        outer.incrementRevision(aTransaction);
        return;
      }
      try {
        var i = index++;
        if (DEBUG) debug("Upgrade step: " + i + "\n");
        steps[i].call(outer);
      } catch(ex) {
        dump("Caught exception" + ex);
        aTransaction.abort();
        return;
      }
    };
    if (aNewVersion > steps.length) {
      dump("Contacts DB upgrade error!");
      aTransaction.abort();
    }
    next();
  },

  makeImport: function makeImport(aContact) {
    let contact = {};
    contact.properties = {
      name:            [],
      honorificPrefix: [],
      givenName:       [],
      additionalName:  [],
      familyName:      [],
      honorificSuffix: [],
      nickname:        [],
      email:           [],
      photo:           [],
      url:             [],
      category:        [],
      adr:             [],
      tel:             [],
      org:             [],
      jobTitle:        [],
      bday:            null,
      note:            [],
      impp:            [],
      anniversary:     null,
      sex:             null,
      genderIdentity:  null,
      key:             [],
    };

    contact.search = {
      givenName:       [],
      familyName:      [],
      email:           [],
      category:        [],
      tel:             [],
      exactTel:        [],
      parsedTel:       [],
    };

    for (let field in aContact.properties) {
      contact.properties[field] = aContact.properties[field];
      // Add search fields
      if (aContact.properties[field] && contact.search[field]) {
        for (let i = 0; i <= aContact.properties[field].length; i++) {
          if (aContact.properties[field][i]) {
            if (field == "tel" && aContact.properties[field][i].value) {
              let number = aContact.properties.tel[i].value.toString();
              let normalized = PhoneNumberUtils.normalize(number);
              // We use an object here to avoid duplicates
              let containsSearch = {};
              let matchSearch = {};

              if (normalized) {
                // exactTel holds normalized version of entered phone number.
                // normalized: +1 (949) 123 - 4567 -> +19491234567
                contact.search.exactTel.push(normalized);
                // matchSearch holds normalized version of entered phone number,
                // nationalNumber, nationalFormat, internationalNumber, internationalFormat
                matchSearch[normalized] = 1;
                let parsedNumber = PhoneNumberUtils.parse(number);
                if (parsedNumber) {
                  if (DEBUG) {
                    debug("InternationalFormat: " + parsedNumber.internationalFormat);
                    debug("InternationalNumber: " + parsedNumber.internationalNumber);
                    debug("NationalNumber: " + parsedNumber.nationalNumber);
                    debug("NationalFormat: " + parsedNumber.nationalFormat);
                  }
                  matchSearch[parsedNumber.nationalNumber] = 1;
                  matchSearch[parsedNumber.internationalNumber] = 1;
                  matchSearch[PhoneNumberUtils.normalize(parsedNumber.nationalFormat)] = 1;
                  matchSearch[PhoneNumberUtils.normalize(parsedNumber.internationalFormat)] = 1;

                  if (this.substringMatching && normalized.length > this.substringMatching) {
                    matchSearch[normalized.slice(-this.substringMatching)] = 1;
                  }
                }

                // containsSearch holds incremental search values for:
                // normalized number and national format
                for (let i = 0; i < normalized.length; i++) {
                  containsSearch[normalized.substring(i, normalized.length)] = 1;
                }
                if (parsedNumber && parsedNumber.nationalFormat) {
                  let number = PhoneNumberUtils.normalize(parsedNumber.nationalFormat);
                  for (let i = 0; i < number.length; i++) {
                    containsSearch[number.substring(i, number.length)] = 1;
                  }
                }
              }
              for (let num in containsSearch) {
                if (num != "null") {
                  contact.search.tel.push(num);
                }
              }
              for (let num in matchSearch) {
                if (num != "null") {
                  contact.search.parsedTel.push(num);
                }
              }
            } else if ((field == "impp" || field == "email") && aContact.properties[field][i].value) {
              let value = aContact.properties[field][i].value;
              if (value && typeof value == "string") {
                contact.search[field].push(value.toLowerCase());
              }
            } else {
              let val = aContact.properties[field][i];
              if (typeof val == "string") {
                contact.search[field].push(val.toLowerCase());
              }
            }
          }
        }
      }
    }
    if (DEBUG) debug("contact:" + JSON.stringify(contact));

    contact.updated = aContact.updated;
    contact.published = aContact.published;
    contact.id = aContact.id;

    return contact;
  },

  updateRecordMetadata: function updateRecordMetadata(record) {
    if (!record.id) {
      Cu.reportError("Contact without ID");
    }
    if (!record.published) {
      record.published = new Date();
    }
    record.updated = new Date();
  },

  removeObjectFromCache: function CDB_removeObjectFromCache(aObjectId, aCallback, aFailureCb) {
    if (DEBUG) debug("removeObjectFromCache: " + aObjectId);
    if (!aObjectId) {
      if (DEBUG) debug("No object ID passed");
      return;
    }
    this.newTxn("readwrite", SAVED_GETALL_STORE_NAME, function(txn, store) {
      store.openCursor().onsuccess = function(e) {
        let cursor = e.target.result;
        if (cursor) {
          for (let i = 0; i < cursor.value.length; ++i) {
            if (cursor.value[i] == aObjectId) {
              if (DEBUG) debug("id matches cache");
              cursor.value.splice(i, 1);
              cursor.update(cursor.value);
              break;
            }
          }
          cursor.continue();
        } else {
          aCallback();
        }
      }.bind(this);
    }.bind(this), null,
    function(errorMsg) {
      aFailureCb(errorMsg);
    });
  },

  // Invalidate the entire cache. It will be incrementally regenerated on demand
  // See getCacheForQuery
  invalidateCache: function CDB_invalidateCache(aErrorCb) {
    if (DEBUG) debug("invalidate cache");
    this.newTxn("readwrite", SAVED_GETALL_STORE_NAME, function (txn, store) {
      store.clear();
    }, aErrorCb);
  },

  incrementRevision: function CDB_incrementRevision(txn) {
    let revStore = txn.objectStore(REVISION_STORE);
    revStore.get(REVISION_KEY).onsuccess = function(e) {
      revStore.put(parseInt(e.target.result, 10) + 1, REVISION_KEY);
    };
  },

  saveContact: function CDB_saveContact(aContact, successCb, errorCb) {
    let contact = this.makeImport(aContact);
    this.newTxn("readwrite", STORE_NAME, function (txn, store) {
      if (DEBUG) debug("Going to update" + JSON.stringify(contact));

      // Look up the existing record and compare the update timestamp.
      // If no record exists, just add the new entry.
      let newRequest = store.get(contact.id);
      newRequest.onsuccess = function (event) {
        if (!event.target.result) {
          if (DEBUG) debug("new record!")
          this.updateRecordMetadata(contact);
          store.put(contact);
        } else {
          if (DEBUG) debug("old record!")
          if (new Date(typeof contact.updated === "undefined" ? 0 : contact.updated) < new Date(event.target.result.updated)) {
            if (DEBUG) debug("rev check fail!");
            txn.abort();
            return;
          } else {
            if (DEBUG) debug("rev check OK");
            contact.published = event.target.result.published;
            contact.updated = new Date();
            store.put(contact);
          }
        }
        this.invalidateCache(errorCb);
      }.bind(this);

      this.incrementRevision(txn);
    }.bind(this), successCb, errorCb);
  },

  removeContact: function removeContact(aId, aSuccessCb, aErrorCb) {
    if (DEBUG) debug("removeContact: " + aId);
    this.removeObjectFromCache(aId, function() {
      this.newTxn("readwrite", STORE_NAME, function(txn, store) {
        store.delete(aId).onsuccess = function() {
          aSuccessCb();
        };
        this.incrementRevision(txn);
      }.bind(this), null, aErrorCb);
    }.bind(this), aErrorCb);
  },

  clear: function clear(aSuccessCb, aErrorCb) {
    this.newTxn("readwrite", STORE_NAME, function (txn, store) {
      if (DEBUG) debug("Going to clear all!");
      store.clear();
      this.incrementRevision(txn);
    }.bind(this), aSuccessCb, aErrorCb);
  },

  createCacheForQuery: function CDB_createCacheForQuery(aQuery, aSuccessCb, aFailureCb) {
    this.find(function (aContacts) {
      if (aContacts) {
        let contactsArray = [];
        for (let i in aContacts) {
          contactsArray.push(aContacts[i]);
        }

        // save contact ids in cache
        this.newTxn("readwrite", SAVED_GETALL_STORE_NAME, function(txn, store) {
          store.put(contactsArray.map(function(el) el.id), aQuery);
        }, null, aFailureCb);

        // send full contacts
        aSuccessCb(contactsArray, true);
      } else {
        aSuccessCb([], true);
      }
    }.bind(this),
    function (aErrorMsg) { aFailureCb(aErrorMsg); },
    JSON.parse(aQuery));
  },

  getCacheForQuery: function CDB_getCacheForQuery(aQuery, aSuccessCb, aFailureCb) {
    if (DEBUG) debug("getCacheForQuery");
    // Here we try to get the cached results for query `aQuery'. If they don't
    // exist, it means the cache was invalidated and needs to be recreated, so
    // we do that. Otherwise, we just return the existing cache.
    this.newTxn("readonly", SAVED_GETALL_STORE_NAME, function(txn, store) {
      let req = store.get(aQuery);
      req.onsuccess = function(e) {
        if (e.target.result) {
          if (DEBUG) debug("cache exists");
          aSuccessCb(e.target.result, false);
        } else {
          if (DEBUG) debug("creating cache for query " + aQuery);
          this.createCacheForQuery(aQuery, aSuccessCb);
        }
      }.bind(this);
      req.onerror = function(e) {
        aFailureCb(e.target.errorMessage);
      };
    }.bind(this), null, aFailureCb);
  },

  sendNow: function CDB_sendNow(aCursorId) {
    if (aCursorId in this._dispatcher) {
      this._dispatcher[aCursorId].sendNow();
    }
  },

  clearDispatcher: function CDB_clearDispatcher(aCursorId) {
    if (DEBUG) debug("clearDispatcher: " + aCursorId);
    if (aCursorId in this._dispatcher) {
      delete this._dispatcher[aCursorId];
    }
  },

  getAll: function CDB_getAll(aSuccessCb, aFailureCb, aOptions, aCursorId) {
    if (DEBUG) debug("getAll")
    let optionStr = JSON.stringify(aOptions);
    this.getCacheForQuery(optionStr, function(aCachedResults, aFullContacts) {
      // aFullContacts is true if the cache didn't exist and had to be created.
      // In that case, we receive the full contacts since we already have them
      // in memory to create the cache. This allows us to avoid accessing the
      // object store again.
      if (aCachedResults && aCachedResults.length > 0) {
        let newTxnFn = this.newTxn.bind(this);
        let clearDispatcherFn = this.clearDispatcher.bind(this, aCursorId);
        this._dispatcher[aCursorId] = new ContactDispatcher(aCachedResults, aFullContacts,
                                                            aSuccessCb, newTxnFn,
                                                            clearDispatcherFn, aFailureCb);
        this._dispatcher[aCursorId].sendNow();
      } else { // no contacts
        if (DEBUG) debug("query returned no contacts");
        aSuccessCb(null);
      }
    }.bind(this), aFailureCb);
  },

  getRevision: function CDB_getRevision(aSuccessCb, aErrorCb) {
    if (DEBUG) debug("getRevision");
    this.newTxn("readonly", REVISION_STORE, function (txn, store) {
      store.get(REVISION_KEY).onsuccess = function (e) {
        aSuccessCb(e.target.result);
      };
    },null, aErrorCb);
  },

  getCount: function CDB_getCount(aSuccessCb, aErrorCb) {
    if (DEBUG) debug("getCount");
    this.newTxn("readonly", STORE_NAME, function (txn, store) {
      store.count().onsuccess = function (e) {
        aSuccessCb(e.target.result);
      };
    }, null, aErrorCb);
  },

  /*
   * Sorting the contacts by sortBy field. aSortBy can either be familyName or givenName.
   * If 2 entries have the same sortyBy field or no sortBy field is present, we continue
   * sorting with the other sortyBy field.
   */
  sortResults: function CDB_sortResults(aResults, aFindOptions) {
    if (!aFindOptions)
      return;
    if (aFindOptions.sortBy != "undefined") {
      const sortOrder = aFindOptions.sortOrder;
      const sortBy = aFindOptions.sortBy == "familyName" ? [ "familyName", "givenName" ] : [ "givenName" , "familyName" ];

      aResults.sort(function (a, b) {
        let x, y;
        let result = 0;
        let xIndex = 0;
        let yIndex = 0;

        do {
          while (xIndex < sortBy.length && !x) {
            x = a.properties[sortBy[xIndex]];
            if (x) {
              x = x.join("").toLowerCase();
            }
            xIndex++;
          }
          while (yIndex < sortBy.length && !y) {
            y = b.properties[sortBy[yIndex]];
            if (y) {
              y = y.join("").toLowerCase();
            }
            yIndex++;
          }
          if (!x) {
            if (!y) {
              let px, py;
              px = JSON.stringify(a.published);
              py = JSON.stringify(b.published);
              if (px && py) {
                return px.localeCompare(py);
              }
            } else {
              return sortOrder == 'descending' ? 1 : -1;
            }
          }
          if (!y) {
            return sortOrder == "ascending" ? 1 : -1;
          }

          result = x.localeCompare(y);
          x = null;
          y = null;
        } while (result == 0);

        return sortOrder == "ascending" ? result : -result;
      });
    }
    if (aFindOptions.filterLimit && aFindOptions.filterLimit != 0) {
      if (DEBUG) debug("filterLimit is set: " + aFindOptions.filterLimit);
      aResults.splice(aFindOptions.filterLimit, aResults.length);
    }
  },

  /**
   * @param successCb
   *        Callback function to invoke with result array.
   * @param failureCb [optional]
   *        Callback function to invoke when there was an error.
   * @param options [optional]
   *        Object specifying search options. Possible attributes:
   *        - filterBy
   *        - filterOp
   *        - filterValue
   *        - count
   */
  find: function find(aSuccessCb, aFailureCb, aOptions) {
    if (DEBUG) debug("ContactDB:find val:" + aOptions.filterValue + " by: " + aOptions.filterBy + " op: " + aOptions.filterOp);
    let self = this;
    this.newTxn("readonly", STORE_NAME, function (txn, store) {
      let filterOps = ["equals", "contains", "match", "startsWith"];
      if (aOptions && (filterOps.indexOf(aOptions.filterOp) >= 0)) {
        self._findWithIndex(txn, store, aOptions);
      } else {
        self._findAll(txn, store, aOptions);
      }
    }, aSuccessCb, aFailureCb);
  },

  _findWithIndex: function _findWithIndex(txn, store, options) {
    if (DEBUG) debug("_findWithIndex: " + options.filterValue +" " + options.filterOp + " " + options.filterBy + " ");
    let fields = options.filterBy;
    for (let key in fields) {
      if (DEBUG) debug("key: " + fields[key]);
      if (!store.indexNames.contains(fields[key]) && fields[key] != "id") {
        if (DEBUG) debug("Key not valid!" + fields[key] + ", " + store.indexNames);
        txn.abort();
        return;
      }
    }

    // lookup for all keys
    if (options.filterBy.length == 0) {
      if (DEBUG) debug("search in all fields!" + JSON.stringify(store.indexNames));
      for(let myIndex = 0; myIndex < store.indexNames.length; myIndex++) {
        fields = Array.concat(fields, store.indexNames[myIndex])
      }
    }

    // Sorting functions takes care of limit if set.
    let limit = options.sortBy === 'undefined' ? options.filterLimit : null;

    let filter_keys = fields.slice();
    for (let key = filter_keys.shift(); key; key = filter_keys.shift()) {
      let request;
      if (key == "id") {
        // store.get would return an object and not an array
        request = store.mozGetAll(options.filterValue);
      } else if (key == "category") {
        let index = store.index(key);
        request = index.mozGetAll(options.filterValue, limit);
      } else if (options.filterOp == "equals") {
        if (DEBUG) debug("Getting index: " + key);
        // case sensitive
        let index = store.index(key);
        let filterValue = options.filterValue;
        if (key == "tel") {
          filterValue = PhoneNumberUtils.normalize(filterValue,
                                                   /*numbersOnly*/ true);
        }
        request = index.mozGetAll(filterValue, limit);
      } else if (options.filterOp == "match") {
        if (DEBUG) debug("match");
        if (key != "tel") {
          dump("ContactDB: 'match' filterOp only works on tel\n");
          return txn.abort();
        }

        let index = store.index("telMatch");
        let normalized = PhoneNumberUtils.normalize(options.filterValue,
                                                    /*numbersOnly*/ true);

        // Some countries need special handling for number matching. Bug 877302
        if (this.substringMatching && normalized.length > this.substringMatching) {
          normalized = normalized.slice(-this.substringMatching);
        }

        if (!normalized.length) {
          dump("ContactDB: normalized filterValue is empty, can't perform match search.\n");
          return txn.abort();
        }

        request = index.mozGetAll(normalized, limit);
      } else {
        // XXX: "contains" should be handled separately, this is "startsWith"
        if (options.filterOp === 'contains' && key !== 'tel') {
          dump("ContactDB: 'contains' only works for 'tel'. Falling back " +
               "to 'startsWith'.\n");
        }
        // not case sensitive
        let lowerCase = options.filterValue.toString().toLowerCase();
        if (key === "tel") {
          let origLength = lowerCase.length;
          let tmp = PhoneNumberUtils.normalize(lowerCase, /*numbersOnly*/ true);
          if (tmp.length != origLength) {
            let NON_SEARCHABLE_CHARS = /[^#+\*\d\s()-]/;
            // e.g. number "123". find with "(123)" but not with "123a"
            if (tmp === "" || NON_SEARCHABLE_CHARS.test(lowerCase)) {
              if (DEBUG) debug("Call continue!");
              continue;
            }
            lowerCase = tmp;
          }
        }
        if (DEBUG) debug("lowerCase: " + lowerCase);
        let range = this._global.IDBKeyRange.bound(lowerCase, lowerCase + "\uFFFF");
        let index = store.index(key + "LowerCase");
        request = index.mozGetAll(range, limit);
      }
      if (!txn.result)
        txn.result = {};

      request.onsuccess = function (event) {
        if (DEBUG) debug("Request successful. Record count: " + event.target.result.length);
        this.sortResults(event.target.result, options);
        for (let i in event.target.result)
          txn.result[event.target.result[i].id] = exportContact(event.target.result[i]);
      }.bind(this);
    }
  },

  _findAll: function _findAll(txn, store, options) {
    if (DEBUG) debug("ContactDB:_findAll:  " + JSON.stringify(options));
    if (!txn.result)
      txn.result = {};
    // Sorting functions takes care of limit if set.
    let limit = options.sortBy === 'undefined' ? options.filterLimit : null;
    store.mozGetAll(null, limit).onsuccess = function (event) {
      if (DEBUG) debug("Request successful. Record count:" + event.target.result.length);
      this.sortResults(event.target.result, options);
      for (let i in event.target.result) {
        txn.result[event.target.result[i].id] = exportContact(event.target.result[i]);
      }
    }.bind(this);
  },

  // Enable special phone number substring matching. Does not update existing DB entries.
  enableSubstringMatching: function enableSubstringMatching(aDigits) {
    if (DEBUG) debug("MCC enabling substring matching " + aDigits);
    this.substringMatching = aDigits;
  },

  disableSubstringMatching: function disableSubstringMatching() {
    if (DEBUG) debug("MCC disabling substring matching");
    delete this.substringMatching;
  },

  init: function init(aGlobal) {
    this.initDBHelper(DB_NAME, DB_VERSION, [STORE_NAME, SAVED_GETALL_STORE_NAME, REVISION_STORE], aGlobal);
  }
};
