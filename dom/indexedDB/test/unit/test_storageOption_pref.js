/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/*
 * Tests that the dom.indexedDB.storageOption.enabled pref is able to
 * control whether the storage option is accepted by indexedDB.open().
 */

var testGenerator = testSteps();

function* testSteps()
{
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("dom.indexedDB.storageOption.enabled");
  });

  const name = "Splendid Test";
  const version = 1;
  const origin = "https://example.com";

  // Avoid trying to show permission prompts on a window.
  let uri = Services.io.newURI(origin);
  Services.perms.add(uri, "indexedDB", Ci.nsIPermissionManager.ALLOW_ACTION);

  const objectStoreName = "Foo";
  const data = { key: 1, value: "bar" };

  // Turn the storage option off, content databases should be "default".
  Services.prefs.setBoolPref("dom.indexedDB.storageOption.enabled", false);

  // Open a database with content privileges.
  let principal = getPrincipal(origin);
  let request = indexedDB.openForPrincipal(principal, name,
    { version, storage: "persistent" });

  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  is(event.type, "upgradeneeded", "Got correct event type");

  let db = event.target.result;
  db.onerror = errorHandler;

  let objectStore = db.createObjectStore(objectStoreName, { });

  event = yield undefined;

  is(event.type, "success", "Got correct event type");

  is(db.name, name, "Correct name");
  is(db.version, version, "Correct version");
  is(db.storage, "default", "Correct persistence type");

  objectStore = db.transaction([objectStoreName], "readwrite")
                  .objectStore(objectStoreName);

  request = objectStore.get(data.key);
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, undefined, "Got no data");

  request = objectStore.add(data.value, data.key);
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, data.key, "Got correct key");

  // Turn the storage option on, content databases should be able to get
  // "persistent" now. Because persistent storage is separate from default
  // storage, we will not find the database we just created and will
  // receive an upgradeneeded event.
  Services.prefs.setBoolPref("dom.indexedDB.storageOption.enabled", true);

  request = indexedDB.openForPrincipal(principal, name,
    { version, storage: "persistent" });

  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.type, "upgradeneeded", "Got correct event type");

  db = event.target.result;
  db.onerror = errorHandler;

  event = yield undefined;

  is(event.type, "success", "Got correct event type");

  is(db.name, name, "Correct name");
  is(db.version, version, "Correct version");
  is(db.storage, "persistent", "Correct persistence type");

  // A new database was created (because it changed persistence type).
  is(db.objectStoreNames.length, 0, "Got no data");

  finishTest();
  yield undefined;
}
