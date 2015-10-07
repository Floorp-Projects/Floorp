/* global
    sendAsyncMessage,
    addMessageListener,
    indexedDB
 */
"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

var imports = {};

Cu.import("resource://gre/modules/ContactDB.jsm", imports);
Cu.import("resource://gre/modules/ContactService.jsm", imports);
Cu.import("resource://gre/modules/Promise.jsm", imports);
Cu.importGlobalProperties(["indexedDB"]);

const {
  STORE_NAME,
  SAVED_GETALL_STORE_NAME,
  REVISION_STORE,
  DB_NAME,
  ContactService,
} = imports;
// |const| will not work because
// it will make the Promise object immutable before assigning.
// Using Object.defineProperty() instead.
Object.defineProperty(this, "Promise", {
  value: imports.Promise, writable: false, configurable: false
});

var DEBUG = false;
function debug(str) {
  if (DEBUG){
    dump("-*- TestMigrationChromeScript: " + str + "\n");
  }
}

const DATA = {
  12: {
    SCHEMA: function createSchema12(db, transaction) {
      let objectStore = db.createObjectStore(STORE_NAME, {keyPath: "id"});
      objectStore.createIndex("familyName", "properties.familyName", { multiEntry: true });
      objectStore.createIndex("givenName",  "properties.givenName",  { multiEntry: true });
      objectStore.createIndex("familyNameLowerCase", "search.familyName", { multiEntry: true });
      objectStore.createIndex("givenNameLowerCase",  "search.givenName",  { multiEntry: true });
      objectStore.createIndex("telLowerCase",        "search.tel",        { multiEntry: true });
      objectStore.createIndex("emailLowerCase",      "search.email",      { multiEntry: true });
      objectStore.createIndex("tel", "search.exactTel", { multiEntry: true });
      objectStore.createIndex("category", "properties.category", { multiEntry: true });
      objectStore.createIndex("email", "search.email", { multiEntry: true });
      objectStore.createIndex("telMatch", "search.parsedTel", {multiEntry: true});
      db.createObjectStore(SAVED_GETALL_STORE_NAME);
      db.createObjectStore(REVISION_STORE).put(0, "revision");
    },
    BAD: [
      {
        // this contact is bad because its "name" property is not an array and
        // is the empty string (upgrade 16 to 17)
        "properties": {
          "name": "",
          "email": [],
          "url": [{
            "type": ["source"],
            "value": "urn:service:gmail:uid:http://www.google.com/m8/feeds/contacts/XXX/base/4567894654"
          }],
          "category": ["gmail"],
          "adr": [],
          "tel": [{
            "type": ["mobile"],
            "value": "+7 123 456-78-90"
          }],
          "sex": "undefined",
          "genderIdentity": "undefined"
        },
        "search": {
          "givenName": [],
          "familyName": [],
          "email": [],
          "category": ["gmail"],
          "tel": ["+71234567890","71234567890","1234567890","234567890","34567890",
            "4567890","567890","67890","7890","890","90","0","81234567890"],
          "exactTel": ["+71234567890"],
          "parsedTel": ["+71234567890","1234567890","81234567890","34567890"]
        },
        "updated": new Date("2013-07-27T16:47:40.974Z"),
        "published": new Date("2013-07-27T16:47:40.974Z"),
        "id": "bad-1"
      },
      {
        // This contact is bad because its "name" property is not an array
        // (upgrade 14 to 15)
        "properties": {
          "name": "name-bad-2",
          "email": [],
          "url": [{
            "type": ["source"],
            "value": "urn:service:gmail:uid:http://www.google.com/m8/feeds/contacts/XXX/base/4567894654"
          }],
          "category": ["gmail"],
          "adr": [],
          "tel": [{
            "type": ["mobile"],
            "value": "+7 123 456-78-90"
          }],
          "sex": "undefined",
          "genderIdentity": "undefined"
        },
        "search": {
          "givenName": [],
          "familyName": [],
          "email": [],
          "category": ["gmail"],
          "tel": ["+71234567890","71234567890","1234567890","234567890","34567890",
            "4567890","567890","67890","7890","890","90","0","81234567890"],
          "exactTel": ["+71234567890"],
          "parsedTel": ["+71234567890","1234567890","81234567890","34567890"]
        },
        "updated": new Date("2013-07-27T16:47:40.974Z"),
        "published": new Date("2013-07-27T16:47:40.974Z"),
        "id": "bad-2"
      },
      {
        // This contact is bad because its bday property is a String (upgrade 15
        // to 16), and its anniversary property is an empty string (upgrade 16
        // to 17)
        "properties": {
          "name": ["name-bad-3"],
          "email": [],
          "url": [{
            "type": ["source"],
            "value": "urn:service:gmail:uid:http://www.google.com/m8/feeds/contacts/XXX/base/4567894654"
          }],
          "category": ["gmail"],
          "adr": [],
          "tel": [{
            "type": ["mobile"],
            "value": "+7 123 456-78-90"
          }],
          "sex": "undefined",
          "genderIdentity": "undefined",
          "bday": "2013-07-27T16:47:40.974Z",
          "anniversary": ""
        },
        "search": {
          "givenName": [],
          "familyName": [],
          "email": [],
          "category": ["gmail"],
          "tel": ["+71234567890","71234567890","1234567890","234567890","34567890",
            "4567890","567890","67890","7890","890","90","0","81234567890"],
          "exactTel": ["+71234567890"],
          "parsedTel": ["+71234567890","1234567890","81234567890","34567890"]
        },
        "updated": new Date("2013-07-27T16:47:40.974Z"),
        "published": new Date("2013-07-27T16:47:40.974Z"),
        "id": "bad-3"
      },
      {
        // This contact is bad because its tel property has a tel.type null
        // value (upgrade 16 to 17), and email.type not array value (upgrade 14
        // to 15)
        "properties": {
          "name": ["name-bad-4"],
          "email": [{
            "value": "toto@toto.com",
            "type": "home"
          }],
          "url": [{
            "type": ["source"],
            "value": "urn:service:gmail:uid:http://www.google.com/m8/feeds/contacts/XXX/base/4567894654"
          }],
          "category": ["gmail"],
          "adr": [],
          "tel": [{
            "type": null,
            "value": "+7 123 456-78-90"
          }],
          "sex": "undefined",
          "genderIdentity": "undefined"
        },
        "search": {
          "givenName": [],
          "familyName": [],
          "email": [],
          "category": ["gmail"],
          "tel": ["+71234567890","71234567890","1234567890","234567890","34567890",
            "4567890","567890","67890","7890","890","90","0","81234567890"],
          "exactTel": ["+71234567890"],
          "parsedTel": ["+71234567890","1234567890","81234567890","34567890"]
        },
        "updated": new Date("2013-07-27T16:47:40.974Z"),
        "published": new Date("2013-07-27T16:47:40.974Z"),
        "id": "bad-4"
      }
    ],
    GOOD: [
      {
        "properties": {
          "name": ["name-good-1"],
          "email": [],
          "url": [{
            "type": ["source"],
            "value": "urn:service:gmail:uid:http://www.google.com/m8/feeds/contacts/XXX/base/4567894654"
          }],
          "category": ["gmail"],
          "adr": [],
          "tel": [{
            "type": ["mobile"],
            "value": "+7 123 456-78-90"
          }],
          "sex": "undefined",
          "genderIdentity": "undefined"
        },
        "search": {
          "givenName": [],
          "familyName": [],
          "email": [],
          "category": ["gmail"],
          "tel": ["+71234567890","71234567890","1234567890","234567890","34567890",
            "4567890","567890","67890","7890","890","90","0","81234567890"],
          "exactTel": ["+71234567890"],
          "parsedTel": ["+71234567890","1234567890","81234567890","34567890"]
        },
        "updated": new Date("2013-07-27T16:47:40.974Z"),
        "published": new Date("2013-07-27T16:47:40.974Z"),
        "id": "good-1"
      }
    ]
  }
};

function DataManager(version) {
  if (!(version in DATA)) {
    throw new Error("Version " + version + " can't be found in our test datas.");
  }

  this.version = version;
  this.data = DATA[version];
}

DataManager.prototype = {
  open: function() {
    debug("opening for version " + this.version);
    var deferred = Promise.defer();

    let req = indexedDB.open(DB_NAME, this.version);
    req.onupgradeneeded = function() {
      let db = req.result;
      let transaction = req.transaction;
      this.createSchema(db, transaction);
      this.addContacts(db, transaction);
    }.bind(this);

    req.onsuccess = function() {
      debug("succeeded opening the db, let's close it now");
      req.result.close();
      deferred.resolve(this.contactsCount());
    }.bind(this);

    req.onerror = function() {
      deferred.reject(this.error);
    };

    return deferred.promise;
  },

  createSchema: function(db, transaction) {
    debug("createSchema for version " + this.version);
    this.data.SCHEMA(db, transaction);
  },

  addContacts: function(db, transaction) {
    debug("adding contacts for version " + this.version);
    var os = transaction.objectStore(STORE_NAME);

    this.data.GOOD.forEach(function(contact) {
      os.put(contact);
    });
    this.data.BAD.forEach(function(contact) {
      os.put(contact);
    });
  },

  contactsCount: function() {
    return this.data.BAD.length + this.data.GOOD.length;
  }
};

DataManager.delete = function() {
  debug("Deleting the database");
  var deferred = Promise.defer();

  /* forcibly close the db before deleting it */
  ContactService._db.close();

  var req = indexedDB.deleteDatabase(DB_NAME);
  req.onsuccess = function() {
    debug("Successfully deleted!");
    deferred.resolve();
  };

  req.onerror = function() {
    debug("Not deleted, error is " + this.error.name);
    deferred.reject(this.error);
  };

  req.onblocked = function() {
    debug("Waiting for the current users");
  };

  return deferred.promise;
};

addMessageListener("createDB", function(version) {
  // Promises help handling gracefully exceptions
  Promise.resolve().then(function() {
    return new DataManager(version);
  }).then(function(manager) {
    return manager.open();
  }).then(function onSuccess(count) {
    sendAsyncMessage("createDB.success", count);
  }, function onError(err) {
    sendAsyncMessage("createDB.error", "Failed to create the DB: " +
      "(" + err.name + ") " + err.message);
  });
});

addMessageListener("deleteDB", function() {
  Promise.resolve().then(
    DataManager.delete
  ).then(function onSuccess() {
    sendAsyncMessage("deleteDB.success");
  }, function onError(err) {
    sendAsyncMessage("deleteDB.error", "Failed to delete the DB:" + err.name);
  });
});
