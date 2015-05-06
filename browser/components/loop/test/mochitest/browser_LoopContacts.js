/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {LoopContacts} = Cu.import("resource:///modules/loop/LoopContacts.jsm", {});
const {LoopStorage} = Cu.import("resource:///modules/loop/LoopStorage.jsm", {});

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

const kContacts = [{
  id: 1,
  name: ["Ally Avocado"],
  email: [{
    "pref": true,
    "type": ["work"],
    "value": "ally@mail.com"
  }],
  tel: [{
    "pref": true,
    "type": ["mobile"],
    "value": "+31-6-12345678"
  }],
  category: ["google"],
  published: 1406798311748,
  updated: 1406798311748
},{
  id: 2,
  name: ["Bob Banana"],
  email: [{
    "pref": true,
    "type": ["work"],
    "value": "bob@gmail.com"
  }],
  tel: [{
    "pref": true,
    "type": ["mobile"],
    "value": "+1-214-5551234"
  }],
  category: ["local"],
  published: 1406798311748,
  updated: 1406798311748
}, {
  id: 3,
  name: ["Caitlin Cantaloupe"],
  email: [{
    "pref": true,
    "type": ["work"],
    "value": "caitlin.cant@hotmail.com"
  }],
  category: ["local"],
  published: 1406798311748,
  updated: 1406798311748
}, {
  id: 4,
  name: ["Dave Dragonfruit"],
  email: [{
    "pref": true,
    "type": ["work"],
    "value": "dd@dragons.net"
  }],
  category: ["google"],
  published: 1406798311748,
  updated: 1406798311748
}];

const kDanglingContact = {
  id: 5,
  name: ["Ellie Eggplant"],
  email: [{
    "pref": true,
    "type": ["work"],
    "value": "ellie@yahoo.com"
  }],
  category: ["google"],
  blocked: true,
  published: 1406798311748,
  updated: 1406798311748
};

const promiseLoadContacts = function() {
  return new Promise((resolve, reject) => {
    LoopContacts.removeAll(err => {
      if (err) {
        reject(err);
        return;
      }

      gExpectedAdds.push(...kContacts);
      LoopContacts.addMany(kContacts, (err, contacts) => {
        if (err) {
          reject(err);
          return;
        }
        resolve(contacts);
      });
    });
  });
};

// Get a copy of a contact without private properties.
const normalizeContact = function(contact) {
  let result = {};
  // Get a copy of contact without private properties.
  for (let prop of Object.getOwnPropertyNames(contact)) {
    if (!prop.startsWith("_")) {
      result[prop] = contact[prop];
    }
  }
  return result;
};

const compareContacts = function(contact1, contact2) {
  Assert.ok("_guid" in contact1, "First contact should have an ID.");
  Assert.deepEqual(normalizeContact(contact1), normalizeContact(contact2));
};

// LoopContacts emits various events. Test if they work as expected here.
let gExpectedAdds = [];
let gExpectedRemovals = [];
let gExpectedUpdates = [];

const onContactAdded = function(e, contact) {
  let expectedIds = gExpectedAdds.map(contact => contact.id);
  let idx = expectedIds.indexOf(contact.id);
  Assert.ok(idx > -1, "Added contact should be expected");
  let expected = gExpectedAdds[idx];
  compareContacts(contact, expected);
  gExpectedAdds.splice(idx, 1);
};

const onContactRemoved = function(e, contact) {
  let idx = gExpectedRemovals.indexOf(contact._guid);
  Assert.ok(idx > -1, "Removed contact should be expected");
  gExpectedRemovals.splice(idx, 1);
};

const onContactUpdated = function(e, contact) {
  let idx = gExpectedUpdates.indexOf(contact._guid);
  Assert.ok(idx > -1, "Updated contact should be expected");
  gExpectedUpdates.splice(idx, 1);
};

LoopContacts.on("add", onContactAdded);
LoopContacts.on("remove", onContactRemoved);
LoopContacts.on("update", onContactUpdated);

registerCleanupFunction(function () {
  LoopContacts.removeAll(() => {});
  LoopContacts.off("add", onContactAdded);
  LoopContacts.off("remove", onContactRemoved);
  LoopContacts.off("update", onContactUpdated);
});

// Test adding a contact.
add_task(function* () {
  let contacts = yield promiseLoadContacts();
  for (let i = 0, l = contacts.length; i < l; ++i) {
    compareContacts(contacts[i], kContacts[i]);
  }

  info("Add a contact.");
  yield new Promise((resolve, reject) => {
    gExpectedAdds.push(kDanglingContact);
    LoopContacts.add(kDanglingContact, (err, contact) => {
      Assert.ok(!err, "There shouldn't be an error");
      compareContacts(contact, kDanglingContact);

      info("Check if it's persisted.");
      LoopContacts.get(contact._guid, (err, contact) => {
        Assert.ok(!err, "There shouldn't be an error");
        compareContacts(contact, kDanglingContact);
        resolve();
      });
    });
  });
});

add_task(function* () {
  info("Test removing all contacts.");
  let contacts = yield promiseLoadContacts();

  yield new Promise((resolve, reject) => {
    LoopContacts.removeAll(function(err) {
      Assert.ok(!err, "There shouldn't be an error");
      LoopContacts.getAll(function(err, found) {
        Assert.ok(!err, "There shouldn't be an error");
        Assert.equal(found.length, 0, "There shouldn't be any contacts left");
        resolve();
      });
    });
  });
});

// Test retrieving a contact.
add_task(function* () {
  let contacts = yield promiseLoadContacts();

  info("Get a single contact.");
  yield new Promise((resolve, reject) => {
    LoopContacts.get(contacts[1]._guid, (err, contact) => {
      Assert.ok(!err, "There shouldn't be an error");
      compareContacts(contact, kContacts[1]);
      resolve();
    });
  });

  info("Get a single contact by id.");
  yield new Promise((resolve, reject) => {
    LoopContacts.getByServiceId(2, (err, contact) => {
      Assert.ok(!err, "There shouldn't be an error");
      compareContacts(contact, kContacts[1]);
      resolve();
    });
  });

  info("Get a couple of contacts.");
  yield new Promise((resolve, reject) => {
    let toRetrieve = [contacts[0], contacts[2], contacts[3]];
    LoopContacts.getMany(toRetrieve.map(contact => contact._guid), (err, result) => {
      Assert.ok(!err, "There shouldn't be an error");
      Assert.equal(result.length, toRetrieve.length, "Result list should be the same " +
                   "size as the list of items to retrieve");

      function resultFilter(c) {
        return c._guid == this._guid;
      }

      for (let contact of toRetrieve) {
        let found = result.filter(resultFilter.bind(contact));
        Assert.ok(found.length, "Contact " + contact._guid + " should be in the list");
        compareContacts(found[0], contact);
      }
      resolve();
    });
  });

  info("Get all contacts.");
  yield new Promise((resolve, reject) => {
    LoopContacts.getAll((err, contacts) => {
      Assert.ok(!err, "There shouldn't be an error");
      for (let i = 0, l = contacts.length; i < l; ++i) {
        compareContacts(contacts[i], kContacts[i]);
      }
      resolve();
    });
  });

  info("Get a non-existent contact.");
  return new Promise((resolve, reject) => {
    LoopContacts.get(1000, (err, contact) => {
      Assert.ok(!err, "There shouldn't be an error");
      Assert.ok(!contact, "There shouldn't be a contact");
      resolve();
    });
  });
});

// Test removing a contact.
add_task(function* () {
  let contacts = yield promiseLoadContacts();

  info("Remove a single contact.");
  yield new Promise((resolve, reject) => {
    let toRemove = contacts[2]._guid;
    gExpectedRemovals.push(toRemove);
    LoopContacts.remove(toRemove, err => {
      Assert.ok(!err, "There shouldn't be an error");

      LoopContacts.get(toRemove, (err, contact) => {
        Assert.ok(!err, "There shouldn't be an error");
        Assert.ok(!contact, "There shouldn't be a contact");
        resolve();
      });
    });
  });

  info("Remove a non-existing contact.");
  yield new Promise((resolve, reject) => {
    LoopContacts.remove(1000, (err, contact) => {
      Assert.ok(!err, "There shouldn't be an error");
      Assert.ok(!contact, "There shouldn't be a contact");
      resolve();
    });
  });

  info("Remove multiple contacts.");
  yield new Promise((resolve, reject) => {
    let toRemove = [contacts[0]._guid, contacts[1]._guid];
    gExpectedRemovals.push(...toRemove);
    LoopContacts.removeMany(toRemove, err => {
      Assert.ok(!err, "There shouldn't be an error");

      LoopContacts.getAll((err, contacts) => {
        Assert.ok(!err, "There shouldn't be an error");
        let ids = contacts.map(contact => contact._guid);
        Assert.equal(ids.indexOf(toRemove[0]), -1, "Contact '" + toRemove[0] +
                                                   "' shouldn't be there");
        Assert.equal(ids.indexOf(toRemove[1]), -1, "Contact '" + toRemove[1] +
                                                   "' shouldn't be there");
        resolve();
      });
    });
  });
});

// Test updating a contact.
add_task(function* () {
  let contacts = yield promiseLoadContacts();

  const newBday = (new Date(403920000000)).toISOString();

  info("Update a single contact.");
  yield new Promise((resolve, reject) => {
    let toUpdate = {
      _guid: contacts[2]._guid,
      bday: newBday
    };
    gExpectedUpdates.push(contacts[2]._guid);
    LoopContacts.update(toUpdate, (err, result) => {
      Assert.ok(!err, "There shouldn't be an error");
      Assert.equal(result, toUpdate._guid, "Result should be the same as the contact ID");

      LoopContacts.get(toUpdate._guid, (err, contact) => {
        Assert.ok(!err, "There shouldn't be an error");
        Assert.equal(contact.bday, newBday, "Birthday should be the same");
        info("Check that all other properties were left intact.");
        contacts[2].bday = newBday;
        compareContacts(contact, contacts[2]);
        resolve();
      });
    });
  });

  info("Update a non-existing contact.");
  yield new Promise((resolve, reject) => {
    let toUpdate = {
      _guid: 1000,
      bday: newBday
    };
    LoopContacts.update(toUpdate, (err, contact) => {
      Assert.ok(err, "There should be an error");
      Assert.equal(err.message, "Contact with _guid '1000' could not be found",
                   "Error message should be correct");
      resolve();
    });
  });
});

// Test blocking and unblocking a contact.
add_task(function* () {
  let contacts = yield promiseLoadContacts();

  info("Block contact.");
  yield new Promise((resolve, reject) => {
    let toBlock = contacts[1]._guid;
    gExpectedUpdates.push(toBlock);
    LoopContacts.block(toBlock, (err, result) => {
      Assert.ok(!err, "There shouldn't be an error");
      Assert.equal(result, toBlock, "Result should be the same as the contact ID");

      LoopContacts.get(toBlock, (err, contact) => {
        Assert.ok(!err, "There shouldn't be an error");
        Assert.strictEqual(contact.blocked, true, "Blocked status should be set");
        info("Check that all other properties were left intact.");
        delete contact.blocked;
        compareContacts(contact, contacts[1]);
        resolve();
      });
    });
  });

  info("Block a non-existing contact.");
  yield new Promise((resolve, reject) => {
    LoopContacts.block(1000, err => {
      Assert.ok(err, "There should be an error");
      Assert.equal(err.message, "Contact with _guid '1000' could not be found",
                   "Error message should be correct");
      resolve();
    });
  });

  info("Unblock a contact.");
  yield new Promise((resolve, reject) => {
    let toUnblock = contacts[1]._guid;
    gExpectedUpdates.push(toUnblock);
    LoopContacts.unblock(toUnblock, (err, result) => {
      Assert.ok(!err, "There shouldn't be an error");
      Assert.equal(result, toUnblock, "Result should be the same as the contact ID");

      LoopContacts.get(toUnblock, (err, contact) => {
        Assert.ok(!err, "There shouldn't be an error");
        Assert.strictEqual(contact.blocked, false, "Blocked status should be set");
        info("Check that all other properties were left intact.");
        delete contact.blocked;
        compareContacts(contact, contacts[1]);
        resolve();
      });
    });
  });

  info("Unblock a non-existing contact.");
  yield new Promise((resolve, reject) => {
    LoopContacts.unblock(1000, err => {
      Assert.ok(err, "There should be an error");
      Assert.equal(err.message, "Contact with _guid '1000' could not be found",
                   "Error message should be correct");
      resolve();
    });
  });
});

// Test if the event emitter implementation doesn't leak and is working as expected.
add_task(function* () {
  yield promiseLoadContacts();

  Assert.strictEqual(gExpectedAdds.length, 0, "No contact additions should be expected anymore");
  Assert.strictEqual(gExpectedRemovals.length, 0, "No contact removals should be expected anymore");
  Assert.strictEqual(gExpectedUpdates.length, 0, "No contact updates should be expected anymore");
});

// Test switching between different databases.
add_task(function* () {
  Assert.equal(LoopStorage.databaseName, "default", "First active partition should be the default");
  yield promiseLoadContacts();

  let uuid = uuidgen.generateUUID().toString().replace(/[{}]+/g, "");
  LoopStorage.switchDatabase(uuid);
  Assert.equal(LoopStorage.databaseName, uuid, "The active partition should have changed");

  yield promiseLoadContacts();

  let contacts = yield promiseLoadContacts();
  for (let i = 0, l = contacts.length; i < l; ++i) {
    compareContacts(contacts[i], kContacts[i]);
  }

  LoopStorage.switchDatabase();
  Assert.equal(LoopStorage.databaseName, "default", "The active partition should have changed");

  contacts = yield LoopContacts.promise("getAll");
  for (let i = 0, l = contacts.length; i < l; ++i) {
    compareContacts(contacts[i], kContacts[i]);
  }
});

// Test searching for contacts.
add_task(function* () {
  yield promiseLoadContacts();

  let contacts = yield LoopContacts.promise("search", {
    q: "bob@gmail.com"
  });
  Assert.equal(contacts.length, 1, "There should be one contact found");
  compareContacts(contacts[0], kContacts[1]);

  // Test searching by name.
  contacts = yield LoopContacts.promise("search", {
    q: "Ally Avocado",
    field: "name"
  });
  Assert.equal(contacts.length, 1, "There should be one contact found");
  compareContacts(contacts[0], kContacts[0]);

  // Test searching for multiple contacts.
  contacts = yield LoopContacts.promise("search", {
    q: "google",
    field: "category"
  });
  Assert.equal(contacts.length, 2, "There should be two contacts found");

  // Test searching for telephone numbers.
  contacts = yield LoopContacts.promise("search", {
    q: "+31612345678",
    field: "tel"
  });
  Assert.equal(contacts.length, 1, "There should be one contact found");
  compareContacts(contacts[0], kContacts[0]);

  // Test searching for telephone numbers without prefixes.
  contacts = yield LoopContacts.promise("search", {
    q: "5551234",
    field: "tel"
  });
  Assert.equal(contacts.length, 1, "There should be one contact found");
  compareContacts(contacts[0], kContacts[1]);
});
