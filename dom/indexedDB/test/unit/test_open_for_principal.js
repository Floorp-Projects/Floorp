/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";

  const objectStoreName = "Foo";

  const data = { key: 1, value: "bar" };

  let request = indexedDB.open(name, 1);
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

  objectStore = db.transaction([objectStoreName], "readwrite")
                  .objectStore(objectStoreName);

  request = objectStore.get(data.key);
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, null, "Got no data");

  request = objectStore.add(data.value, data.key);
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, data.key, "Got correct key");

  let uri = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService)
                      .newURI("http://appdata.example.com", null, null);
  let principal = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                     .getService(Components.interfaces.nsIScriptSecurityManager)
                     .getNoAppCodebasePrincipal(uri);

  request = indexedDB.openForPrincipal(principal, name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  is(event.type, "upgradeneeded", "Got correct event type");

  db = event.target.result;
  db.onerror = errorHandler;

  objectStore = db.createObjectStore(objectStoreName, { });

  event = yield undefined;

  is(event.type, "success", "Got correct event type");

  objectStore = db.transaction([objectStoreName])
                  .objectStore(objectStoreName);

  request = objectStore.get(data.key);
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, null, "Got no data");

  db.close();

  request = indexedDB.deleteForPrincipal(principal, name);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler
  event = yield undefined;

  finishTest();
  yield undefined;
}
