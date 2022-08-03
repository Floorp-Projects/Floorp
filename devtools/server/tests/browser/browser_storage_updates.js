/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure that storage updates are detected and that the correct information is
// contained inside the storage actors.

"use strict";

/* import-globals-from storage-helpers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/server/tests/browser/storage-helpers.js",
  this
);

const TESTS = [
  // index 0
  {
    async action(win) {
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
      "local-storage": [
        {
          name: "l1",
          value: "foobar1",
        },
      ],
    },
  },

  // index 1
  {
    async action() {
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
      "local-storage": [
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
    async action() {
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
      "local-storage": [
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
    async action() {
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
      "local-storage": [
        {
          name: "l3",
          value: "new_foobar3",
        },
      ],
      "session-storage": [
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
    async action() {
      await sessionStorageRemoveItem("s1");
    },
    snapshot: {
      cookies: [
        {
          name: "c3",
          value: "foobar3",
        },
      ],
      "local-storage": [
        {
          name: "l3",
          value: "new_foobar3",
        },
      ],
      "session-storage": [
        {
          name: "s2",
          value: "foobar2",
        },
      ],
    },
  },

  // index 5
  {
    async action() {
      await clearCookies();
    },
    snapshot: {
      cookies: [],
      "local-storage": [
        {
          name: "l3",
          value: "new_foobar3",
        },
      ],
      "session-storage": [
        {
          name: "s2",
          value: "foobar2",
        },
      ],
    },
  },

  // index 6
  {
    async action() {
      await clearLocalAndSessionStores();
    },
    snapshot: {
      cookies: [],
      "local-storage": [],
      "session-storage": [],
    },
  },
];

add_task(async function() {
  const { commands } = await openTabAndSetupStorage(
    MAIN_DOMAIN + "storage-updates.html"
  );

  for (let i = 0; i < TESTS.length; i++) {
    const test = TESTS[i];
    await runTest(test, commands, i);
  }

  await commands.destroy();
});

async function runTest({ action, snapshot }, commands, index) {
  info("Running test at index " + index);
  await action();
  await checkStores(commands, snapshot);
}

async function checkStores(commands, snapshot) {
  const { resourceCommand } = commands;
  const { TYPES } = resourceCommand;
  const actual = {};
  await resourceCommand.watchResources(
    [TYPES.COOKIE, TYPES.LOCAL_STORAGE, TYPES.SESSION_STORAGE],
    {
      async onAvailable(resources) {
        for (const resource of resources) {
          actual[resource.resourceType] = await resource.getStoreObjects(
            TEST_DOMAIN
          );
        }
      },
    }
  );

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

async function addCookie(name, value) {
  info(`addCookie("${name}", "${value}")`);

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [[name, value]],
    ([iName, iValue]) => {
      content.wrappedJSObject.window.addCookie(iName, iValue);
    }
  );
}

async function removeCookie(name) {
  info(`removeCookie("${name}")`);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [name], iName => {
    content.wrappedJSObject.window.removeCookie(iName);
  });
}

async function localStorageSetItem(name, value) {
  info(`localStorageSetItem("${name}", "${value}")`);

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [[name, value]],
    ([iName, iValue]) => {
      content.window.localStorage.setItem(iName, iValue);
    }
  );
}

async function localStorageRemoveItem(name) {
  info(`localStorageRemoveItem("${name}")`);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [name], iName => {
    content.window.localStorage.removeItem(iName);
  });
}

async function sessionStorageSetItem(name, value) {
  info(`sessionStorageSetItem("${name}", "${value}")`);

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [[name, value]],
    ([iName, iValue]) => {
      content.window.sessionStorage.setItem(iName, iValue);
    }
  );
}

async function sessionStorageRemoveItem(name) {
  info(`sessionStorageRemoveItem("${name}")`);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [name], iName => {
    content.window.sessionStorage.removeItem(iName);
  });
}

async function clearCookies() {
  info(`clearCookies()`);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.window.clearCookies();
  });
}

async function clearLocalAndSessionStores() {
  info(`clearLocalAndSessionStores()`);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.window.clearLocalAndSessionStores();
  });
}
