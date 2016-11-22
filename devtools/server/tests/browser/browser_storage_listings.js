/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {StorageFront} = require("devtools/shared/fronts/storage");
Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/devtools/server/tests/browser/storage-helpers.js", this);

const storeMap = {
  cookies: {
    "test1.example.org": [
      {
        name: "c1",
        value: "foobar",
        expires: 2000000000000,
        path: "/browser",
        host: "test1.example.org",
        isDomain: false,
        isSecure: false,
      },
      {
        name: "cs2",
        value: "sessionCookie",
        path: "/",
        host: ".example.org",
        expires: 0,
        isDomain: true,
        isSecure: false,
      },
      {
        name: "c3",
        value: "foobar-2",
        expires: 2000000001000,
        path: "/",
        host: "test1.example.org",
        isDomain: false,
        isSecure: true,
      }
    ],
    "sectest1.example.org": [
      {
        name: "uc1",
        value: "foobar",
        host: ".example.org",
        path: "/",
        expires: 0,
        isDomain: true,
        isSecure: true,
      },
      {
        name: "cs2",
        value: "sessionCookie",
        path: "/",
        host: ".example.org",
        expires: 0,
        isDomain: true,
        isSecure: false,
      },
      {
        name: "sc1",
        value: "foobar",
        path: "/browser/devtools/server/tests/browser/",
        host: "sectest1.example.org",
        expires: 0,
        isDomain: false,
        isSecure: false,
      }
    ]
  },
  localStorage: {
    "http://test1.example.org": [
      {
        name: "ls1",
        value: "foobar"
      },
      {
        name: "ls2",
        value: "foobar-2"
      }
    ],
    "http://sectest1.example.org": [
      {
        name: "iframe-u-ls1",
        value: "foobar"
      }
    ],
    "https://sectest1.example.org": [
      {
        name: "iframe-s-ls1",
        value: "foobar"
      }
    ]
  },
  sessionStorage: {
    "http://test1.example.org": [
      {
        name: "ss1",
        value: "foobar-3"
      }
    ],
    "http://sectest1.example.org": [
      {
        name: "iframe-u-ss1",
        value: "foobar1"
      },
      {
        name: "iframe-u-ss2",
        value: "foobar2"
      }
    ],
    "https://sectest1.example.org": [
      {
        name: "iframe-s-ss1",
        value: "foobar-2"
      }
    ]
  }
};

const IDBValues = {
  listStoresResponse: {
    "http://test1.example.org": [
      ["idb1 (default)", "obj1"], ["idb1 (default)", "obj2"], ["idb2 (default)", "obj3"]
    ],
    "http://sectest1.example.org": [
    ],
    "https://sectest1.example.org": [
      ["idb-s1 (default)", "obj-s1"], ["idb-s2 (default)", "obj-s2"]
    ]
  },
  dbDetails: {
    "http://test1.example.org": [
      {
        db: "idb1 (default)",
        origin: "http://test1.example.org",
        version: 1,
        objectStores: 2
      },
      {
        db: "idb2 (default)",
        origin: "http://test1.example.org",
        version: 1,
        objectStores: 1
      },
    ],
    "http://sectest1.example.org": [
    ],
    "https://sectest1.example.org": [
      {
        db: "idb-s1 (default)",
        origin: "https://sectest1.example.org",
        version: 1,
        objectStores: 1
      },
      {
        db: "idb-s2 (default)",
        origin: "https://sectest1.example.org",
        version: 1,
        objectStores: 1
      },
    ]
  },
  objectStoreDetails: {
    "http://test1.example.org": {
      "idb1 (default)": [
        {
          objectStore: "obj1",
          keyPath: "id",
          autoIncrement: false,
          indexes: [
            {
              name: "name",
              keyPath: "name",
              "unique": false,
              multiEntry: false,
            },
            {
              name: "email",
              keyPath: "email",
              "unique": true,
              multiEntry: false,
            },
          ]
        },
        {
          objectStore: "obj2",
          keyPath: "id2",
          autoIncrement: false,
          indexes: []
        }
      ],
      "idb2 (default)": [
        {
          objectStore: "obj3",
          keyPath: "id3",
          autoIncrement: false,
          indexes: [
            {
              name: "name2",
              keyPath: "name2",
              "unique": true,
              multiEntry: false,
            }
          ]
        },
      ]
    },
    "http://sectest1.example.org" : {},
    "https://sectest1.example.org": {
      "idb-s1 (default)": [
        {
          objectStore: "obj-s1",
          keyPath: "id",
          autoIncrement: false,
          indexes: []
        },
      ],
      "idb-s2 (default)": [
        {
          objectStore: "obj-s2",
          keyPath: "id3",
          autoIncrement: true,
          indexes: [
            {
              name: "name2",
              keyPath: "name2",
              "unique": true,
              multiEntry: false,
            }
          ]
        },
      ]
    }

  },
  entries: {
    "http://test1.example.org": {
      "idb1 (default)#obj1": [
        {
          name: 1,
          value: {
            id: 1,
            name: "foo",
            email: "foo@bar.com",
          }
        },
        {
          name: 2,
          value: {
            id: 2,
            name: "foo2",
            email: "foo2@bar.com",
          }
        },
        {
          name: 3,
          value: {
            id: 3,
            name: "foo2",
            email: "foo3@bar.com",
          }
        }
      ],
      "idb1 (default)#obj2": [
        {
          name: 1,
          value: {
            id2: 1,
            name: "foo",
            email: "foo@bar.com",
            extra: "baz"
          }
        }
      ],
      "idb2 (default)#obj3": []
    },
    "http://sectest1.example.org" : {},
    "https://sectest1.example.org": {
      "idb-s1 (default)#obj-s1": [
        {
          name: 6,
          value: {
            id: 6,
            name: "foo",
            email: "foo@bar.com",
          }
        },
        {
          name: 7,
          value: {
            id: 7,
            name: "foo2",
            email: "foo2@bar.com",
          }
        }
      ],
      "idb-s2 (default)#obj-s2": [
        {
          name: 13,
          value: {
            id2: 13,
            name2: "foo",
            email: "foo@bar.com",
          }
        }
      ]
    }
  }
};

function finishTests(client) {

  let closeConnection = () => {

  };
}

function* testStores(data) {
  ok(data.cookies, "Cookies storage actor is present");
  ok(data.localStorage, "Local Storage storage actor is present");
  ok(data.sessionStorage, "Session Storage storage actor is present");
  ok(data.indexedDB, "Indexed DB storage actor is present");
  yield testCookies(data.cookies);
  yield testLocalStorage(data.localStorage);
  yield testSessionStorage(data.sessionStorage);
  yield testIndexedDB(data.indexedDB);
}

function testCookies(cookiesActor) {
  is(Object.keys(cookiesActor.hosts).length, 2,
                 "Correct number of host entries for cookies");
  return testCookiesObjects(0, cookiesActor.hosts, cookiesActor);
}

var testCookiesObjects = Task.async(function* (index, hosts, cookiesActor) {
  let host = Object.keys(hosts)[index];
  let matchItems = data => {
    let cookiesLength = 0;
    for (let secureCookie of storeMap.cookies[host]) {
      if (secureCookie.isSecure) {
        ++cookiesLength;
      }
    }
    // Any secure cookies did not get stored in the database.
    is(data.total, storeMap.cookies[host].length - cookiesLength,
       "Number of cookies in host " + host + " matches");
    for (let item of data.data) {
      let found = false;
      for (let toMatch of storeMap.cookies[host]) {
        if (item.name == toMatch.name) {
          found = true;
          ok(true, "Found cookie " + item.name + " in response");
          is(item.value.str, toMatch.value, "The value matches.");
          is(item.expires, toMatch.expires, "The expiry time matches.");
          is(item.path, toMatch.path, "The path matches.");
          is(item.host, toMatch.host, "The host matches.");
          is(item.isSecure, toMatch.isSecure, "The isSecure value matches.");
          is(item.isDomain, toMatch.isDomain, "The isDomain value matches.");
          break;
        }
      }
      ok(found, "cookie " + item.name + " should exist in response");
    }
  };

  ok(!!storeMap.cookies[host], "Host is present in the list : " + host);
  matchItems(yield cookiesActor.getStoreObjects(host));
  if (index == Object.keys(hosts).length - 1) {
    return;
  }
  yield testCookiesObjects(++index, hosts, cookiesActor);
});

function testLocalStorage(localStorageActor) {
  is(Object.keys(localStorageActor.hosts).length, 3,
     "Correct number of host entries for local storage");
  return testLocalStorageObjects(0, localStorageActor.hosts, localStorageActor);
}

var testLocalStorageObjects = Task.async(function* (index, hosts, localStorageActor) {
  let host = Object.keys(hosts)[index];
  let matchItems = data => {
    is(data.total, storeMap.localStorage[host].length,
       "Number of local storage items in host " + host + " matches");
    for (let item of data.data) {
      let found = false;
      for (let toMatch of storeMap.localStorage[host]) {
        if (item.name == toMatch.name) {
          found = true;
          ok(true, "Found local storage item " + item.name + " in response");
          is(item.value.str, toMatch.value, "The value matches.");
          break;
        }
      }
      ok(found, "local storage item " + item.name + " should exist in response");
    }
  };

  ok(!!storeMap.localStorage[host], "Host is present in the list : " + host);
  matchItems(yield localStorageActor.getStoreObjects(host));
  if (index == Object.keys(hosts).length - 1) {
    return;
  }
  yield testLocalStorageObjects(++index, hosts, localStorageActor);
});

function testSessionStorage(sessionStorageActor) {
  is(Object.keys(sessionStorageActor.hosts).length, 3,
     "Correct number of host entries for session storage");
  return testSessionStorageObjects(0, sessionStorageActor.hosts,
                                   sessionStorageActor);
}

var testSessionStorageObjects = Task.async(function* (index, hosts, sessionStorageActor) {
  let host = Object.keys(hosts)[index];
  let matchItems = data => {
    is(data.total, storeMap.sessionStorage[host].length,
       "Number of session storage items in host " + host + " matches");
    for (let item of data.data) {
      let found = false;
      for (let toMatch of storeMap.sessionStorage[host]) {
        if (item.name == toMatch.name) {
          found = true;
          ok(true, "Found session storage item " + item.name + " in response");
          is(item.value.str, toMatch.value, "The value matches.");
          break;
        }
      }
      ok(found, "session storage item " + item.name + " should exist in response");
    }
  };

  ok(!!storeMap.sessionStorage[host], "Host is present in the list : " + host);
  matchItems(yield sessionStorageActor.getStoreObjects(host));
  if (index == Object.keys(hosts).length - 1) {
    return;
  }
  yield testSessionStorageObjects(++index, hosts, sessionStorageActor);
});

var testIndexedDB = Task.async(function* (indexedDBActor) {
  is(Object.keys(indexedDBActor.hosts).length, 3,
     "Correct number of host entries for indexed db");

  for (let host in indexedDBActor.hosts) {
    for (let item of indexedDBActor.hosts[host]) {
      let parsedItem = JSON.parse(item);
      let found = false;
      for (let toMatch of IDBValues.listStoresResponse[host]) {
        if (toMatch[0] == parsedItem[0] && toMatch[1] == parsedItem[1]) {
          found = true;
          break;
        }
      }
      ok(found, item + " should exist in list stores response");
    }
  }

  yield testIndexedDBs(0, indexedDBActor.hosts, indexedDBActor);
  yield testObjectStores(0, indexedDBActor.hosts, indexedDBActor);
  yield testIDBEntries(0, indexedDBActor.hosts, indexedDBActor);
});

var testIndexedDBs = Task.async(function* (index, hosts, indexedDBActor) {
  let host = Object.keys(hosts)[index];
  let matchItems = data => {
    is(data.total, IDBValues.dbDetails[host].length,
       "Number of indexed db in host " + host + " matches");
    for (let item of data.data) {
      let found = false;
      for (let toMatch of IDBValues.dbDetails[host]) {
        if (item.db == toMatch.db) {
          found = true;
          ok(true, "Found indexed db " + item.db + " in response");
          is(item.origin, toMatch.origin, "The origin matches.");
          is(item.version, toMatch.version, "The version matches.");
          is(item.objectStores, toMatch.objectStores,
             "The numebr of object stores matches.");
          break;
        }
      }
      ok(found, "indexed db " + item.name + " should exist in response");
    }
  };

  ok(!!IDBValues.dbDetails[host], "Host is present in the list : " + host);
  matchItems(yield indexedDBActor.getStoreObjects(host));
  if (index == Object.keys(hosts).length - 1) {
    return;
  }
  yield testIndexedDBs(++index, hosts, indexedDBActor);
});

var testObjectStores = Task.async(function* (index, hosts, indexedDBActor) {
  let host = Object.keys(hosts)[index];
  let matchItems = (data, db) => {
    is(data.total, IDBValues.objectStoreDetails[host][db].length,
       "Number of object stores in host " + host + " matches");
    for (let item of data.data) {
      let found = false;
      for (let toMatch of IDBValues.objectStoreDetails[host][db]) {
        if (item.objectStore == toMatch.objectStore) {
          found = true;
          ok(true, "Found object store " + item.objectStore + " in response");
          is(item.keyPath, toMatch.keyPath, "The keyPath matches.");
          is(item.autoIncrement, toMatch.autoIncrement, "The autoIncrement matches.");
          item.indexes = JSON.parse(item.indexes);
          is(item.indexes.length, toMatch.indexes.length, "Number of indexes match");
          for (let index of item.indexes) {
            let indexFound = false;
            for (let toMatchIndex of toMatch.indexes) {
              if (toMatchIndex.name == index.name) {
                indexFound = true;
                ok(true, "Found index " + index.name);
                is(index.keyPath, toMatchIndex.keyPath,
                   "The keyPath of index matches.");
                is(index.unique, toMatchIndex.unique, "The unique matches");
                is(index.multiEntry, toMatchIndex.multiEntry,
                   "The multiEntry matches");
                break;
              }
            }
            ok(indexFound, "Index " + index + " should exist in response");
          }
          break;
        }
      }
      ok(found, "indexed db " + item.name + " should exist in response");
    }
  };

  ok(!!IDBValues.objectStoreDetails[host], "Host is present in the list : " + host);
  for (let name of hosts[host]) {
    let objName = JSON.parse(name).slice(0, 1);
    matchItems((
      yield indexedDBActor.getStoreObjects(host, [JSON.stringify(objName)])
    ), objName[0]);
  }
  if (index == Object.keys(hosts).length - 1) {
    return;
  }
  yield testObjectStores(++index, hosts, indexedDBActor);
});

var testIDBEntries = Task.async(function* (index, hosts, indexedDBActor) {
  let host = Object.keys(hosts)[index];
  let matchItems = (data, obj) => {
    is(data.total, IDBValues.entries[host][obj].length,
       "Number of items in object store " + obj + " matches");
    for (let item of data.data) {
      let found = false;
      for (let toMatch of IDBValues.entries[host][obj]) {
        if (item.name == toMatch.name) {
          found = true;
          ok(true, "Found indexed db item " + item.name + " in response");
          let value = JSON.parse(item.value.str);
          is(Object.keys(value).length, Object.keys(toMatch.value).length,
             "Number of entries in the value matches");
          for (let key in value) {
            is(value[key], toMatch.value[key],
               "value of " + key + " value key matches");
          }
          break;
        }
      }
      ok(found, "indexed db item " + item.name + " should exist in response");
    }
  };

  ok(!!IDBValues.entries[host], "Host is present in the list : " + host);
  for (let name of hosts[host]) {
    let parsed = JSON.parse(name);
    matchItems((
      yield indexedDBActor.getStoreObjects(host, [name])
    ), parsed[0] + "#" + parsed[1]);
  }
  if (index == Object.keys(hosts).length - 1) {
    return;
  }
  yield testObjectStores(++index, hosts, indexedDBActor);
});

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let front = StorageFront(client, form);
  let data = yield front.listStores();
  yield testStores(data);

  yield clearStorage();

  // Forcing GC/CC to get rid of docshells and windows created by this test.
  forceCollections();
  yield client.close();
  forceCollections();
  DebuggerServer.destroy();
  forceCollections();
});
