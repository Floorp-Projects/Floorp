/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {GoogleImporter} = Cu.import("resource:///modules/loop/GoogleImporter.jsm", {});

let importer = new GoogleImporter();

function promiseImport() {
  return new Promise(function(resolve, reject) {
    importer.startImport({}, function(err, stats) {
      if (err) {
        reject(err);
      } else {
        resolve(stats);
      }
    }, mockDb, window);
  });
}

const kContactsCount = 7;

add_task(function* test_GoogleImport() {
  let stats;
  // An error may throw and the test will fail when that happens.
  stats = yield promiseImport();

  let contactsCount = mockDb.size;

  // Assert the world.
  Assert.equal(stats.total, contactsCount, "Five contacts should get processed");
  Assert.equal(stats.success, contactsCount, "Five contacts should be imported");

  yield promiseImport();
  Assert.equal(Object.keys(mockDb._store).length, contactsCount, "Database should be the same size after reimport");

  let currentContact = contactsCount;

  let c = mockDb._store[mockDb._next_guid - currentContact];
  Assert.equal(c.name[0], "John Smith", "Full name should match");
  Assert.equal(c.givenName[0], "John", "Given name should match");
  Assert.equal(c.familyName[0], "Smith", "Family name should match");
  Assert.equal(c.email[0].type, "other", "Email type should match");
  Assert.equal(c.email[0].value, "john.smith@example.com", "Email should match");
  Assert.equal(c.email[0].pref, true, "Pref should match");
  Assert.equal(c.category[0], "google", "Category should match");
  Assert.equal(c.id, "http://www.google.com/m8/feeds/contacts/tester%40mochi.com/base/0", "UID should match and be scoped to provider");

  c = mockDb._store[mockDb._next_guid - (--currentContact)];
  Assert.equal(c.name[0], "Jane Smith", "Full name should match");
  Assert.equal(c.givenName[0], "Jane", "Given name should match");
  Assert.equal(c.familyName[0], "Smith", "Family name should match");
  Assert.equal(c.email[0].type, "other", "Email type should match");
  Assert.equal(c.email[0].value, "jane.smith@example.com", "Email should match");
  Assert.equal(c.email[0].pref, true, "Pref should match");
  Assert.equal(c.category[0], "google", "Category should match");
  Assert.equal(c.id, "http://www.google.com/m8/feeds/contacts/tester%40mochi.com/base/1", "UID should match and be scoped to provider");

  c = mockDb._store[mockDb._next_guid - (--currentContact)];
  Assert.equal(c.name[0], "Davy Randall Jones", "Full name should match");
  Assert.equal(c.givenName[0], "Davy Randall", "Given name should match");
  Assert.equal(c.familyName[0], "Jones", "Family name should match");
  Assert.equal(c.email[0].type, "other", "Email type should match");
  Assert.equal(c.email[0].value, "davy.jones@example.com", "Email should match");
  Assert.equal(c.email[0].pref, true, "Pref should match");
  Assert.equal(c.category[0], "google", "Category should match");
  Assert.equal(c.id, "http://www.google.com/m8/feeds/contacts/tester%40mochi.com/base/2", "UID should match and be scoped to provider");

  c = mockDb._store[mockDb._next_guid - (--currentContact)];
  Assert.equal(c.name[0], "noname@example.com", "Full name should match");
  Assert.equal(c.email[0].type, "other", "Email type should match");
  Assert.equal(c.email[0].value, "noname@example.com", "Email should match");
  Assert.equal(c.email[0].pref, true, "Pref should match");
  Assert.equal(c.category[0], "google", "Category should match");
  Assert.equal(c.id, "http://www.google.com/m8/feeds/contacts/tester%40mochi.com/base/3", "UID should match and be scoped to provider");

  c = mockDb._store[mockDb._next_guid - (--currentContact)];
  Assert.equal(c.name[0], "lycnix", "Full name should match");
  Assert.equal(c.email[0].type, "other", "Email type should match");
  Assert.equal(c.email[0].value, "lycnix", "Email should match");
  Assert.equal(c.email[0].pref, true, "Pref should match");
  Assert.equal(c.category[0], "google", "Category should match");
  Assert.equal(c.id, "http://www.google.com/m8/feeds/contacts/tester%40mochi.com/base/7", "UID should match and be scoped to provider");

  c = mockDb._store[mockDb._next_guid - (--currentContact)];
  Assert.equal(c.name[0], "+31-6-12345678", "Full name should match");
  Assert.equal(c.tel[0].type, "mobile", "Phone type should match");
  Assert.equal(c.tel[0].value, "+31-6-12345678", "Phone should match");
  Assert.equal(c.tel[0].pref, false, "Pref should match");
  Assert.equal(c.category[0], "google", "Category should match");
  Assert.equal(c.id, "http://www.google.com/m8/feeds/contacts/tester%40mochi.com/base/8", "UID should match and be scoped to provider");

  c = mockDb._store[mockDb._next_guid - (--currentContact)];
  Assert.equal(c.name[0], "215234523452345", "Full name should match");
  Assert.equal(c.tel[0].type, "mobile", "Phone type should match");
  Assert.equal(c.tel[0].value, "215234523452345", "Phone should match");
  Assert.equal(c.tel[0].pref, false, "Pref should match");
  Assert.equal(c.category[0], "google", "Category should match");
  Assert.equal(c.id, "http://www.google.com/m8/feeds/contacts/tester%40mochi.com/base/6", "UID should match and be scoped to provider");
});
