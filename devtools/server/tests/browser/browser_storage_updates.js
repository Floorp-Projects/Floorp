/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {StorageFront} = require("devtools/shared/fronts/storage");

const TESTS = [
  // index 0
  {
    action: async function(win) {
      info('addCookie("c1", "foobar1")');
      await addCookie("c1", "foobar1");

      info('addCookie("c2", "foobar2")');
      await addCookie("c2", "foobar2");

      info('localStorageSetItem("l1", "foobar1")');
      await localStorageSetItem("l1", "foobar1");
    },
    expected: {
      added: {
        cookies: {
          "http://test1.example.org": [
            getCookieId("c1", "test1.example.org",
                        "/browser/devtools/server/tests/browser/"),
            getCookieId("c2", "test1.example.org",
                        "/browser/devtools/server/tests/browser/")
          ]
        },
        localStorage: {
          "http://test1.example.org": ["l1"]
        }
      }
    }
  },

  // index 1
  {
    action: async function() {
      info('addCookie("c1", "new_foobar1")');
      await addCookie("c1", "new_foobar1");

      info('localStorageSetItem("l2", "foobar2")');
      await localStorageSetItem("l2", "foobar2");
    },
    expected: {
      changed: {
        cookies: {
          "http://test1.example.org": [
            getCookieId("c1", "test1.example.org",
                        "/browser/devtools/server/tests/browser/"),
          ]
        }
      },
      added: {
        localStorage: {
          "http://test1.example.org": ["l2"]
        }
      }
    }
  },

  // index 2
  {
    action: async function() {
      info('removeCookie("c2")');
      await removeCookie("c2");

      info('localStorageRemoveItem("l1")');
      await localStorageRemoveItem("l1");

      info('localStorageSetItem("l3", "foobar3")');
      await localStorageSetItem("l3", "foobar3");
    },
    expected: {
      deleted: {
        cookies: {
          "http://test1.example.org": [
            getCookieId("c2", "test1.example.org",
                        "/browser/devtools/server/tests/browser/"),
          ]
        },
        localStorage: {
          "http://test1.example.org": ["l1"]
        }
      },
      added: {
        localStorage: {
          "http://test1.example.org": ["l3"]
        }
      }
    }
  },

  // index 3
  {
    action: async function() {
      info('removeCookie("c1")');
      await removeCookie("c1");

      info('addCookie("c3", "foobar3")');
      await addCookie("c3", "foobar3");

      info('localStorageRemoveItem("l2")');
      await localStorageRemoveItem("l2");

      info('sessionStorageSetItem("s1", "foobar1")');
      await sessionStorageSetItem("s1", "foobar1");

      info('sessionStorageSetItem("s2", "foobar2")');
      await sessionStorageSetItem("s2", "foobar2");

      info('localStorageSetItem("l3", "new_foobar3")');
      await localStorageSetItem("l3", "new_foobar3");
    },
    expected: {
      added: {
        cookies: {
          "http://test1.example.org": [
            getCookieId("c3", "test1.example.org",
                        "/browser/devtools/server/tests/browser/"),
          ]
        },
        sessionStorage: {
          "http://test1.example.org": ["s1", "s2"]
        }
      },
      changed: {
        localStorage: {
          "http://test1.example.org": ["l3"]
        }
      },
      deleted: {
        cookies: {
          "http://test1.example.org": [
            getCookieId("c1", "test1.example.org",
                        "/browser/devtools/server/tests/browser/"),
          ]
        },
        localStorage: {
          "http://test1.example.org": ["l2"]
        }
      }
    }
  },

  // index 4
  {
    action: async function() {
      info('sessionStorageRemoveItem("s1")');
      await sessionStorageRemoveItem("s1");
    },
    expected: {
      deleted: {
        sessionStorage: {
          "http://test1.example.org": ["s1"]
        }
      }
    }
  },

  // index 5
  {
    action: async function() {
      info("clearCookies()");
      await clearCookies();
    },
    expected: {
      deleted: {
        cookies: {
          "http://test1.example.org": [
            getCookieId("c3", "test1.example.org",
                        "/browser/devtools/server/tests/browser/"),
          ]
        }
      }
    }
  }
];

add_task(async function() {
  await addTab(MAIN_DOMAIN + "storage-updates.html");

  initDebuggerServer();

  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);
  const front = StorageFront(client, form);

  await front.listStores();

  for (let i = 0; i < TESTS.length; i++) {
    const test = TESTS[i];
    await runTest(test, front, i);
  }

  await testClearLocalAndSessionStores(front);
  await finishTests(client);
});

function markOutMatched(toBeEmptied, data) {
  if (!Object.keys(toBeEmptied).length) {
    info("Object empty");
    return;
  }
  ok(Object.keys(data).length, "At least one storage type should be present");

  for (const storageType in toBeEmptied) {
    if (!data[storageType]) {
      continue;
    }
    info("Testing for " + storageType);
    for (const host in data[storageType]) {
      ok(toBeEmptied[storageType][host], "Host " + host + " found");

      for (const item of data[storageType][host]) {
        const index = toBeEmptied[storageType][host].indexOf(item);
        ok(index > -1, "Item found - " + item);
        if (index > -1) {
          toBeEmptied[storageType][host].splice(index, 1);
        }
      }
      if (!toBeEmptied[storageType][host].length) {
        delete toBeEmptied[storageType][host];
      }
    }
    if (!Object.keys(toBeEmptied[storageType]).length) {
      delete toBeEmptied[storageType];
    }
  }
}

function onStoresUpdate(expected, {added, changed, deleted}, index) {
  info("inside stores update for index " + index);

  // Here, added, changed and deleted might be null even if they are required as
  // per expected. This is fine as they might come in the next stores-update
  // call or have already come in the previous one.
  if (added) {
    info("matching added object for index " + index);
    markOutMatched(expected.added, added);
  }
  if (changed) {
    info("matching changed object for index " + index);
    markOutMatched(expected.changed, changed);
  }
  if (deleted) {
    info("matching deleted object for index " + index);
    markOutMatched(expected.deleted, deleted);
  }

  if ((!expected.added || !Object.keys(expected.added).length) &&
      (!expected.changed || !Object.keys(expected.changed).length) &&
      (!expected.deleted || !Object.keys(expected.deleted).length)) {
    info("Everything expected has been received for index " + index);
  } else {
    info("Still some updates pending for index " + index);
  }
}

async function runTest({action, expected}, front, index) {
  const update = front.once("stores-update");

  info("Running test at index " + index);
  await action();

  const addedChangedDeleted = await update;

  onStoresUpdate(expected, addedChangedDeleted, index);
}

async function testClearLocalAndSessionStores(front) {
  // We need to wait until we have received stores-cleared for both local and
  // session storage.
  let localStorage = false;
  let sessionStorage = false;

  await clearLocalAndSessionStores();

  let data = await front.once("stores-cleared");

  storesCleared(data);

  if (data.localStorage) {
    localStorage = true;
  }

  data = await front.once("stores-cleared");

  if (data.sessionStorage) {
    sessionStorage = true;
  }

  ok(localStorage, "localStorage was cleared");
  ok(sessionStorage, "sessionStorage was cleared");
}

function storesCleared(data) {
  if (data.sessionStorage || data.localStorage) {
    const hosts = data.sessionStorage || data.localStorage;
    info("Stores cleared required for session storage");
    is(hosts.length, 1, "number of hosts is 1");
    is(hosts[0], "http://test1.example.org",
       "host matches for " + Object.keys(data)[0]);
  } else {
    ok(false, "Stores cleared should only be for local and session storage");
  }
}

async function finishTests(client) {
  await client.close();
  DebuggerServer.destroy();
  finish();
}

async function addCookie(name, value) {
  await ContentTask.spawn(gBrowser.selectedBrowser, [name, value], ([iName, iValue]) => {
    content.wrappedJSObject.window.addCookie(iName, iValue);
  });
}

async function removeCookie(name) {
  await ContentTask.spawn(gBrowser.selectedBrowser, name, iName => {
    content.wrappedJSObject.window.removeCookie(iName);
  });
}

async function localStorageSetItem(name, value) {
  await ContentTask.spawn(gBrowser.selectedBrowser, [name, value], ([iName, iValue]) => {
    content.window.localStorage.setItem(iName, iValue);
  });
}

async function localStorageRemoveItem(name) {
  await ContentTask.spawn(gBrowser.selectedBrowser, name, iName => {
    content.window.localStorage.removeItem(iName);
  });
}

async function sessionStorageSetItem(name, value) {
  await ContentTask.spawn(gBrowser.selectedBrowser, [name, value], ([iName, iValue]) => {
    content.window.sessionStorage.setItem(iName, iValue);
  });
}

async function sessionStorageRemoveItem(name) {
  await ContentTask.spawn(gBrowser.selectedBrowser, name, iName => {
    content.window.sessionStorage.removeItem(iName);
  });
}

async function clearCookies() {
  await ContentTask.spawn(gBrowser.selectedBrowser, {}, () => {
    content.wrappedJSObject.window.clearCookies();
  });
}

async function clearLocalAndSessionStores() {
  await ContentTask.spawn(gBrowser.selectedBrowser, {}, () => {
    content.wrappedJSObject.window.clearLocalAndSessionStores();
  });
}
