/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure that storage updates are detected and that the correct information is
// contained inside the storage actors.

"use strict";

const TESTS = [
  // index 0
  {
    action: async function(win) {
      await addCookie("c1", "foobar1");
      await addCookie("c2", "foobar2");
      await localStorageSetItem("l1", "foobar1");
    },
    snapshot: {
      cookies: [
        {
          name: "c1",
          value: "foobar1",
        },
        {
          name: "c2",
          value: "foobar2",
        },
      ],
      localStorage: [
        {
          name: "l1",
          value: "foobar1",
        },
      ],
    },
  },

  // index 1
  {
    action: async function() {
      await addCookie("c1", "new_foobar1");
      await localStorageSetItem("l2", "foobar2");
    },
    snapshot: {
      cookies: [
        {
          name: "c1",
          value: "new_foobar1",
        },
        {
          name: "c2",
          value: "foobar2",
        },
      ],
      localStorage: [
        {
          name: "l1",
          value: "foobar1",
        },
        {
          name: "l2",
          value: "foobar2",
        },
      ],
    },
  },

  // index 2
  {
    action: async function() {
      await removeCookie("c2");
      await localStorageRemoveItem("l1");
      await localStorageSetItem("l3", "foobar3");
    },
    snapshot: {
      cookies: [
        {
          name: "c1",
          value: "new_foobar1",
        },
      ],
      localStorage: [
        {
          name: "l2",
          value: "foobar2",
        },
        {
          name: "l3",
          value: "foobar3",
        },
      ],
    },
  },

  // index 3
  {
    action: async function() {
      await removeCookie("c1");
      await addCookie("c3", "foobar3");
      await localStorageRemoveItem("l2");
      await sessionStorageSetItem("s1", "foobar1");
      await sessionStorageSetItem("s2", "foobar2");
      await localStorageSetItem("l3", "new_foobar3");
    },
    snapshot: {
      cookies: [
        {
          name: "c3",
          value: "foobar3",
        },
      ],
      localStorage: [
        {
          name: "l3",
          value: "new_foobar3",
        },
      ],
      sessionStorage: [
        {
          name: "s1",
          value: "foobar1",
        },
        {
          name: "s2",
          value: "foobar2",
        },
      ],
    },
  },

  // index 4
  {
    action: async function() {
      await sessionStorageRemoveItem("s1");
    },
    snapshot: {
      cookies: [
        {
          name: "c3",
          value: "foobar3",
        },
      ],
      localStorage: [
        {
          name: "l3",
          value: "new_foobar3",
        },
      ],
      sessionStorage: [
        {
          name: "s2",
          value: "foobar2",
        },
      ],
    },
  },

  // index 5
  {
    action: async function() {
      await clearCookies();
    },
    snapshot: {
      cookies: [],
      localStorage: [
        {
          name: "l3",
          value: "new_foobar3",
        },
      ],
      sessionStorage: [
        {
          name: "s2",
          value: "foobar2",
        },
      ],
    },
  },

  // index 6
  {
    action: async function() {
      await clearLocalAndSessionStores();
    },
    snapshot: {
      cookies: [],
      localStorage: [],
      sessionStorage: [],
    },
  },
];

add_task(async function() {
  const target = await addTabTarget(MAIN_DOMAIN + "storage-updates.html");
  const front = await target.getFront("storage");

  for (let i = 0; i < TESTS.length; i++) {
    const test = TESTS[i];
    await runTest(test, front, i);
  }

  await finishTests(target);
});

async function runTest({ action, snapshot }, front, index) {
  info("Running test at index " + index);
  await action();
  await checkStores(front, snapshot);
}

async function checkStores(front, snapshot) {
  const host = TEST_DOMAIN;
  const stores = await front.listStores();
  const actual = {
    cookies: await stores.cookies.getStoreObjects(host),
    localStorage: await stores.localStorage.getStoreObjects(host),
    sessionStorage: await stores.sessionStorage.getStoreObjects(host),
  };

  for (const [type, entries] of Object.entries(snapshot)) {
    const store = actual[type].data;

    is(
      store.length,
      entries.length,
      `The number of entries in ${type} is correct`
    );

    for (const entry of entries) {
      checkStoreValue(entry.name, entry.value, store);
    }
  }
}

function checkStoreValue(name, value, store) {
  for (const entry of store) {
    if (entry.name === name) {
      ok(true, `There is an entry for "${name}"`);

      // entry.value is a longStringActor so we need to read it's value using
      // entry.value.str.
      is(entry.value.str, value, `Value for ${name} is correct`);
      return;
    }
  }
  ok(false, `There is an entry for "${name}"`);
}

async function finishTests(target) {
  await target.destroy();
  DebuggerServer.destroy();
  finish();
}

async function addCookie(name, value) {
  info(`addCookie("${name}", "${value}")`);

  await ContentTask.spawn(
    gBrowser.selectedBrowser,
    [name, value],
    ([iName, iValue]) => {
      content.wrappedJSObject.window.addCookie(iName, iValue);
    }
  );
}

async function removeCookie(name) {
  info(`removeCookie("${name}")`);

  await ContentTask.spawn(gBrowser.selectedBrowser, name, iName => {
    content.wrappedJSObject.window.removeCookie(iName);
  });
}

async function localStorageSetItem(name, value) {
  info(`localStorageSetItem("${name}", "${value}")`);

  await ContentTask.spawn(
    gBrowser.selectedBrowser,
    [name, value],
    ([iName, iValue]) => {
      content.window.localStorage.setItem(iName, iValue);
    }
  );
}

async function localStorageRemoveItem(name) {
  info(`localStorageRemoveItem("${name}")`);

  await ContentTask.spawn(gBrowser.selectedBrowser, name, iName => {
    content.window.localStorage.removeItem(iName);
  });
}

async function sessionStorageSetItem(name, value) {
  info(`sessionStorageSetItem("${name}", "${value}")`);

  await ContentTask.spawn(
    gBrowser.selectedBrowser,
    [name, value],
    ([iName, iValue]) => {
      content.window.sessionStorage.setItem(iName, iValue);
    }
  );
}

async function sessionStorageRemoveItem(name) {
  info(`sessionStorageRemoveItem("${name}")`);

  await ContentTask.spawn(gBrowser.selectedBrowser, name, iName => {
    content.window.sessionStorage.removeItem(iName);
  });
}

async function clearCookies() {
  info(`clearCookies()`);

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, () => {
    content.wrappedJSObject.window.clearCookies();
  });
}

async function clearLocalAndSessionStores() {
  info(`clearLocalAndSessionStores()`);

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, () => {
    content.wrappedJSObject.window.clearLocalAndSessionStores();
  });
}
