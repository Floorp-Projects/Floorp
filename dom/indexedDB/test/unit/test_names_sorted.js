/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const objectStoreInfo = [
    { name: "foo", options: { keyPath: "id" }, location: 1 },
    { name: "bar", options: { keyPath: "id" }, location: 0 },
  ];
  const indexInfo = [
    { name: "foo", keyPath: "value", location: 1 },
    { name: "bar", keyPath: "value", location: 0 },
  ];

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  let event = yield;
  let db = event.target.result;

  for (let i = 0; i < objectStoreInfo.length; i++) {
    let info = objectStoreInfo[i];
    let objectStore = info.hasOwnProperty("options") ?
                      db.createObjectStore(info.name, info.options) :
                      db.createObjectStore(info.name);

    // Test index creation, and that it ends up in indexNames.
    let objectStoreName = info.name;
    for (let j = 0; j < indexInfo.length; j++) {
      let info = indexInfo[j];
      let count = objectStore.indexNames.length;
      let index = info.hasOwnProperty("options") ?
                  objectStore.createIndex(info.name, info.keyPath,
                                          info.options) :
                  objectStore.createIndex(info.name, info.keyPath);
    }
  }

  request.onsuccess = grabEventAndContinueHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;

  event = yield;

  let objectStoreNames = []
  for (let i = 0; i < objectStoreInfo.length; i++) {
    let info = objectStoreInfo[i];
    objectStoreNames.push(info.name);

    is(db.objectStoreNames[info.location], info.name,
       "Got objectStore name in the right location");

    let trans = db.transaction(info.name);
    let objectStore = trans.objectStore(info.name);
    for (let j = 0; j < indexInfo.length; j++) {
      let info = indexInfo[j];
      is(objectStore.indexNames[info.location], info.name,
         "Got index name in the right location");
    }
  }

  let trans = db.transaction(objectStoreNames);
  for (let i = 0; i < objectStoreInfo.length; i++) {
    let info = objectStoreInfo[i];
  
    is(trans.objectStoreNames[info.location], info.name,
       "Got objectStore name in the right location");
  }

  db.close();

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;
  let event = yield;

  let db = event.target.result;

  let objectStoreNames = []
  for (let i = 0; i < objectStoreInfo.length; i++) {
    let info = objectStoreInfo[i];
    objectStoreNames.push(info.name);

    is(db.objectStoreNames[info.location], info.name,
       "Got objectStore name in the right location");

    let trans = db.transaction(info.name);
    let objectStore = trans.objectStore(info.name);
    for (let j = 0; j < indexInfo.length; j++) {
      let info = indexInfo[j];
      is(objectStore.indexNames[info.location], info.name,
         "Got index name in the right location");
    }
  }

  let trans = db.transaction(objectStoreNames);
  for (let i = 0; i < objectStoreInfo.length; i++) {
    let info = objectStoreInfo[i];
  
    is(trans.objectStoreNames[info.location], info.name,
       "Got objectStore name in the right location");
  }

  db.close();

  finishTest();
  yield;
}
