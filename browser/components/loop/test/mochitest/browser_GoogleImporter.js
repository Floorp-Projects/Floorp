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

add_task(function* test_GoogleImport() {
  let stats;
  // An error may throw and the test will fail when that happens.
  stats = yield promiseImport();

  // Assert the world.
  Assert.equal(stats.total, 5, "Five contacts should get processed");
  Assert.equal(stats.success, 5, "Five contacts should be imported");

  yield promiseImport();
  Assert.equal(Object.keys(mockDb._store).length, 5, "Database should contain only five contact after reimport");

  let c = mockDb._store[mockDb._next_guid - 5];
  Assert.equal(c.name[0], "John Smith", "Full name should match");
  Assert.equal(c.givenName[0], "John", "Given name should match");
  Assert.equal(c.familyName[0], "Smith", "Family name should match");
  Assert.equal(c.email[0].type, "other", "Email type should match");
  Assert.equal(c.email[0].value, "john.smith@example.com", "Email should match");
  Assert.equal(c.email[0].pref, true, "Pref should match");
  Assert.equal(c.category[0], "google", "Category should match");
  Assert.equal(c.id, "http://www.google.com/m8/feeds/contacts/tester%40mochi.com/base/0", "UID should match and be scoped to provider");

  c = mockDb._store[mockDb._next_guid - 4];
  Assert.equal(c.name[0], "Jane Smith", "Full name should match");
  Assert.equal(c.givenName[0], "Jane", "Given name should match");
  Assert.equal(c.familyName[0], "Smith", "Family name should match");
  Assert.equal(c.email[0].type, "other", "Email type should match");
  Assert.equal(c.email[0].value, "jane.smith@example.com", "Email should match");
  Assert.equal(c.email[0].pref, true, "Pref should match");
  Assert.equal(c.category[0], "google", "Category should match");
  Assert.equal(c.id, "http://www.google.com/m8/feeds/contacts/tester%40mochi.com/base/1", "UID should match and be scoped to provider");

  c = mockDb._store[mockDb._next_guid - 3];
  Assert.equal(c.name[0], "Davy Randall Jones", "Full name should match");
  Assert.equal(c.givenName[0], "Davy Randall", "Given name should match");
  Assert.equal(c.familyName[0], "Jones", "Family name should match");
  Assert.equal(c.email[0].type, "other", "Email type should match");
  Assert.equal(c.email[0].value, "davy.jones@example.com", "Email should match");
  Assert.equal(c.email[0].pref, true, "Pref should match");
  Assert.equal(c.category[0], "google", "Category should match");
  Assert.equal(c.id, "http://www.google.com/m8/feeds/contacts/tester%40mochi.com/base/2", "UID should match and be scoped to provider");

  c = mockDb._store[mockDb._next_guid - 2];
  Assert.equal(c.name[0], "noname@example.com", "Full name should match");
  Assert.equal(c.email[0].type, "other", "Email type should match");
  Assert.equal(c.email[0].value, "noname@example.com", "Email should match");
  Assert.equal(c.email[0].pref, true, "Pref should match");
  Assert.equal(c.category[0], "google", "Category should match");
  Assert.equal(c.id, "http://www.google.com/m8/feeds/contacts/tester%40mochi.com/base/3", "UID should match and be scoped to provider");

  c = mockDb._store[mockDb._next_guid - 1];
  Assert.equal(c.name[0], "lycnix", "Full name should match");
  Assert.equal(c.email[0].type, "other", "Email type should match");
  Assert.equal(c.email[0].value, "lycnix", "Email should match");
  Assert.equal(c.email[0].pref, true, "Pref should match");
  Assert.equal(c.category[0], "google", "Category should match");
  Assert.equal(c.id, "http://www.google.com/m8/feeds/contacts/tester%40mochi.com/base/7", "UID should match and be scoped to provider");
});
