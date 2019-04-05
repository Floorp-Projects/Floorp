/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from storage-helpers.js */
Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/devtools/server/tests/browser/storage-helpers.js", this);

const storeMap = {
  cookies: {
    "http://test1.example.org": [
      {
        name: "c1",
        value: "foobar",
        expires: 2000000000000,
        path: "/browser",
        host: "test1.example.org",
        hostOnly: true,
        isSecure: false,
      },
      {
        name: "cs2",
        value: "sessionCookie",
        path: "/",
        host: ".example.org",
        expires: 0,
        hostOnly: false,
        isSecure: false,
      },
      {
        name: "c3",
        value: "foobar-2",
        expires: 2000000001000,
        path: "/",
        host: "test1.example.org",
        hostOnly: true,
        isSecure: true,
      },
    ],

    "http://sectest1.example.org": [
      {
        name: "cs2",
        value: "sessionCookie",
        path: "/",
        host: ".example.org",
        expires: 0,
        hostOnly: false,
        isSecure: false,
      },
      {
        name: "sc1",
        value: "foobar",
        path: "/browser/devtools/server/tests/browser/",
        host: "sectest1.example.org",
        expires: 0,
        hostOnly: true,
        isSecure: false,
      },
    ],

    "https://sectest1.example.org": [
      {
        name: "uc1",
        value: "foobar",
        host: ".example.org",
        path: "/",
        expires: 0,
        hostOnly: false,
        isSecure: true,
      },
      {
        name: "cs2",
        value: "sessionCookie",
        path: "/",
        host: ".example.org",
        expires: 0,
        hostOnly: false,
        isSecure: false,
      },
      {
        name: "sc1",
        value: "foobar",
        path: "/browser/devtools/server/tests/browser/",
        host: "sectest1.example.org",
        expires: 0,
        hostOnly: true,
        isSecure: false,
      },
    ],
  },
  localStorage: {
    "http://test1.example.org": [
      {
        name: "ls1",
        value: "foobar",
      },
      {
        name: "ls2",
        value: "foobar-2",
      },
    ],
    "http://sectest1.example.org": [
      {
        name: "iframe-u-ls1",
        value: "foobar",
      },
    ],
    "https://sectest1.example.org": [
      {
        name: "iframe-s-ls1",
        value: "foobar",
      },
    ],
  },
  sessionStorage: {
    "http://test1.example.org": [
      {
        name: "ss1",
        value: "foobar-3",
      },
    ],
    "http://sectest1.example.org": [
      {
        name: "iframe-u-ss1",
        value: "foobar1",
      },
      {
        name: "iframe-u-ss2",
        value: "foobar2",
      },
    ],
    "https://sectest1.example.org": [
      {
        name: "iframe-s-ss1",
        value: "foobar-2",
      },
    ],
  },
};

const IDBValues = {
  listStoresResponse: {
    "http://test1.example.org": [
      ["idb1 (default)", "obj1"], ["idb1 (default)", "obj2"], ["idb2 (default)", "obj3"],
    ],
    "http://sectest1.example.org": [
    ],
    "https://sectest1.example.org": [
      ["idb-s1 (default)", "obj-s1"], ["idb-s2 (default)", "obj-s2"],
    ],
  },
  dbDetails: {
    "http://test1.example.org": [
      {
        db: "idb1 (default)",
        origin: "http://test1.example.org",
        version: 1,
        objectStores: 2,
      },
      {
        db: "idb2 (default)",
        origin: "http://test1.example.org",
        version: 1,
        objectStores: 1,
      },
    ],
    "http://sectest1.example.org": [
    ],
    "https://sectest1.example.org": [
      {
        db: "idb-s1 (default)",
        origin: "https://sectest1.example.org",
        version: 1,
        objectStores: 1,
      },
      {
        db: "idb-s2 (default)",
        origin: "https://sectest1.example.org",
        version: 1,
        objectStores: 1,
      },
    ],
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
          ],
        },
        {
          objectStore: "obj2",
          keyPath: "id2",
          autoIncrement: false,
          indexes: [],
        },
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
            },
          ],
        },
      ],
    },
    "http://sectest1.example.org": {},
    "https://sectest1.example.org": {
      "idb-s1 (default)": [
        {
          objectStore: "obj-s1",
          keyPath: "id",
          autoIncrement: false,
          indexes: [],
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
            },
          ],
        },
      ],
    },

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
          },
        },
        {
          name: 2,
          value: {
            id: 2,
            name: "foo2",
            email: "foo2@bar.com",
          },
        },
        {
          name: 3,
          value: {
            id: 3,
            name: "foo2",
            email: "foo3@bar.com",
          },
        },
      ],
      "idb1 (default)#obj2": [
        {
          name: 1,
          value: {
            id2: 1,
            name: "foo",
            email: "foo@bar.com",
            extra: "baz",
          },
        },
      ],
      "idb2 (default)#obj3": [],
    },
    "http://sectest1.example.org": {},
    "https://sectest1.example.org": {
      "idb-s1 (default)#obj-s1": [
        {
          name: 6,
          value: {
            id: 6,
            name: "foo",
            email: "foo@bar.com",
          },
        },
        {
          name: 7,
          value: {
            id: 7,
            name: "foo2",
            email: "foo2@bar.com",
          },
        },
      ],
      "idb-s2 (default)#obj-s2": [
        {
          name: 13,
          value: {
            id2: 13,
            name2: "foo",
            email: "foo@bar.com",
          },
        },
      ],
    },
  },
};

async function testStores(data) {
  ok(data.cookies, "Cookies storage actor is present");
  ok(data.localStorage, "Local Storage storage actor is present");
  ok(data.sessionStorage, "Session Storage storage actor is present");
  ok(data.indexedDB, "Indexed DB storage actor is present");
  await testCookies(data.cookies);
  await testLocalStorage(data.localStorage);
  await testSessionStorage(data.sessionStorage);
  await testIndexedDB(data.indexedDB);
}

function testCookies(cookiesActor) {
  is(Object.keys(cookiesActor.hosts).length, 3,
                 "Correct number of host entries for cookies");
  return testCookiesObjects(0, cookiesActor.hosts, cookiesActor);
}

var testCookiesObjects = async function(index, hosts, cookiesActor) {
  const host = Object.keys(hosts)[index];
  const matchItems = data => {
    let cookiesLength = 0;
    for (const secureCookie of storeMap.cookies[host]) {
      if (secureCookie.isSecure) {
        ++cookiesLength;
      }
    }
    // Any secure cookies did not get stored in the database.
    is(data.total, storeMap.cookies[host].length - cookiesLength,
       "Number of cookies in host " + host + " matches");
    for (const item of data.data) {
      let found = false;
      for (const toMatch of storeMap.cookies[host]) {
        if (item.name == toMatch.name) {
          found = true;
          ok(true, "Found cookie " + item.name + " in response");
          is(item.value.str, toMatch.value, "The value matches.");
          is(item.expires, toMatch.expires, "The expiry time matches.");
          is(item.path, toMatch.path, "The path matches.");
          is(item.host, toMatch.host, "The host matches.");
          is(item.isSecure, toMatch.isSecure, "The isSecure value matches.");
          is(item.hostOnly, toMatch.hostOnly, "The hostOnly value matches.");
          break;
        }
      }
      ok(found, "cookie " + item.name + " should exist in response");
    }
  };

  ok(!!storeMap.cookies[host], "Host is present in the list : " + host);
  matchItems(await cookiesActor.getStoreObjects(host));
  if (index == Object.keys(hosts).length - 1) {
    return;
  }
  await testCookiesObjects(++index, hosts, cookiesActor);
};

function testLocalStorage(localStorageActor) {
  is(Object.keys(localStorageActor.hosts).length, 3,
     "Correct number of host entries for local storage");
  return testLocalStorageObjects(0, localStorageActor.hosts, localStorageActor);
}

var testLocalStorageObjects = async function(index, hosts, localStorageActor) {
  const host = Object.keys(hosts)[index];
  const matchItems = data => {
    is(data.total, storeMap.localStorage[host].length,
       "Number of local storage items in host " + host + " matches");
    for (const item of data.data) {
      let found = false;
      for (const toMatch of storeMap.localStorage[host]) {
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
  matchItems(await localStorageActor.getStoreObjects(host));
  if (index == Object.keys(hosts).length - 1) {
    return;
  }
  await testLocalStorageObjects(++index, hosts, localStorageActor);
};

function testSessionStorage(sessionStorageActor) {
  is(Object.keys(sessionStorageActor.hosts).length, 3,
     "Correct number of host entries for session storage");
  return testSessionStorageObjects(0, sessionStorageActor.hosts,
                                   sessionStorageActor);
}

var testSessionStorageObjects = async function(index, hosts, sessionStorageActor) {
  const host = Object.keys(hosts)[index];
  const matchItems = data => {
    is(data.total, storeMap.sessionStorage[host].length,
       "Number of session storage items in host " + host + " matches");
    for (const item of data.data) {
      let found = false;
      for (const toMatch of storeMap.sessionStorage[host]) {
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
  matchItems(await sessionStorageActor.getStoreObjects(host));
  if (index == Object.keys(hosts).length - 1) {
    return;
  }
  await testSessionStorageObjects(++index, hosts, sessionStorageActor);
};

var testIndexedDB = async function(indexedDBActor) {
  // If this test is run with chrome debugging enabled we get an extra
  // key for "chrome". We don't want the test to fail in this case, so
  // ignore it.
  delete indexedDBActor.hosts.chrome;

  is(Object.keys(indexedDBActor.hosts).length, 3,
     "Correct number of host entries for indexed db");

  for (const host in indexedDBActor.hosts) {
    for (const item of indexedDBActor.hosts[host]) {
      const parsedItem = JSON.parse(item);
      let found = false;
      for (const toMatch of IDBValues.listStoresResponse[host]) {
        if (toMatch[0] == parsedItem[0] && toMatch[1] == parsedItem[1]) {
          found = true;
          break;
        }
      }
      ok(found, item + " should exist in list stores response");
    }
  }

  await testIndexedDBs(0, indexedDBActor.hosts, indexedDBActor);
  await testObjectStores(0, indexedDBActor.hosts, indexedDBActor);
  await testIDBEntries(0, indexedDBActor.hosts, indexedDBActor);
};

var testIndexedDBs = async function(index, hosts, indexedDBActor) {
  const host = Object.keys(hosts)[index];
  const matchItems = data => {
    is(data.total, IDBValues.dbDetails[host].length,
       "Number of indexed db in host " + host + " matches");
    for (const item of data.data) {
      let found = false;
      for (const toMatch of IDBValues.dbDetails[host]) {
        if (item.uniqueKey == toMatch.db) {
          found = true;
          ok(true, "Found indexed db " + item.uniqueKey + " in response");
          is(item.origin, toMatch.origin, "The origin matches.");
          is(item.version, toMatch.version, "The version matches.");
          is(item.objectStores, toMatch.objectStores,
             "The number of object stores matches.");
          break;
        }
      }
      ok(found, "indexed db " + item.uniqueKey + " should exist in response");
    }
  };

  ok(!!IDBValues.dbDetails[host], "Host is present in the list : " + host);
  matchItems(await indexedDBActor.getStoreObjects(host));
  if (index == Object.keys(hosts).length - 1) {
    return;
  }
  await testIndexedDBs(++index, hosts, indexedDBActor);
};

var testObjectStores = async function(ix, hosts, indexedDBActor) {
  const host = Object.keys(hosts)[ix];
  const matchItems = (data, db) => {
    is(data.total, IDBValues.objectStoreDetails[host][db].length,
       "Number of object stores in host " + host + " matches");
    for (const item of data.data) {
      let found = false;
      for (const toMatch of IDBValues.objectStoreDetails[host][db]) {
        if (item.objectStore == toMatch.objectStore) {
          found = true;
          ok(true, "Found object store " + item.objectStore + " in response");
          is(item.keyPath, toMatch.keyPath, "The keyPath matches.");
          is(item.autoIncrement, toMatch.autoIncrement, "The autoIncrement matches.");
          item.indexes = JSON.parse(item.indexes);
          is(item.indexes.length, toMatch.indexes.length, "Number of indexes match");
          for (const index of item.indexes) {
            let indexFound = false;
            for (const toMatchIndex of toMatch.indexes) {
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
  for (const name of hosts[host]) {
    const objName = JSON.parse(name).slice(0, 1);
    matchItems((
      await indexedDBActor.getStoreObjects(host, [JSON.stringify(objName)])
    ), objName[0]);
  }
  if (ix == Object.keys(hosts).length - 1) {
    return;
  }
  await testObjectStores(++ix, hosts, indexedDBActor);
};

var testIDBEntries = async function(index, hosts, indexedDBActor) {
  const host = Object.keys(hosts)[index];
  const matchItems = (data, obj) => {
    is(data.total, IDBValues.entries[host][obj].length,
       "Number of items in object store " + obj + " matches");
    for (const item of data.data) {
      let found = false;
      for (const toMatch of IDBValues.entries[host][obj]) {
        if (item.name == toMatch.name) {
          found = true;
          ok(true, "Found indexed db item " + item.name + " in response");
          const value = JSON.parse(item.value.str);
          is(Object.keys(value).length, Object.keys(toMatch.value).length,
             "Number of entries in the value matches");
          for (const key in value) {
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
  for (const name of hosts[host]) {
    const parsed = JSON.parse(name);
    matchItems((
      await indexedDBActor.getStoreObjects(host, [name])
    ), parsed[0] + "#" + parsed[1]);
  }
  if (index == Object.keys(hosts).length - 1) {
    return;
  }
  await testObjectStores(++index, hosts, indexedDBActor);
};

add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["privacy.documentCookies.maxage", 0]]});

  const { target, front } =
    await openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html");

  const data = await front.listStores();
  await testStores(data);

  await clearStorage();

  // Forcing GC/CC to get rid of docshells and windows created by this test.
  forceCollections();
  await target.destroy();
  forceCollections();
  DebuggerServer.destroy();
  forceCollections();
});
