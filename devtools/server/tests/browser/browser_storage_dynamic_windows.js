/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {StorageFront} = require("devtools/shared/fronts/storage");
Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/devtools/server/tests/browser/storage-helpers.js", this);

const beforeReload = {
  cookies: {
    "test1.example.org": ["c1", "cs2", "c3", "uc1"],
    "sectest1.example.org": ["uc1", "cs2"]
  },
  localStorage: {
    "http://test1.example.org": ["ls1", "ls2"],
    "http://sectest1.example.org": ["iframe-u-ls1"]
  },
  sessionStorage: {
    "http://test1.example.org": ["ss1"],
    "http://sectest1.example.org": ["iframe-u-ss1", "iframe-u-ss2"]
  },
  indexedDB: {
    "http://test1.example.org": [
      JSON.stringify(["idb1", "obj1"]),
      JSON.stringify(["idb1", "obj2"]),
      JSON.stringify(["idb2", "obj3"]),
    ],
    "http://sectest1.example.org": []
  }
};

function* testStores(data, front) {
  testWindowsBeforeReload(data);

  // FIXME: Bug 1183581 - browser_storage_dynamic_windows.js IsSafeToRunScript
  //                      errors when testing reload in E10S mode
  // yield testReload(front);
  yield testAddIframe(front);
  yield testRemoveIframe(front);
}

function testWindowsBeforeReload(data) {
  for (let storageType in beforeReload) {
    ok(data[storageType], storageType + " storage actor is present");
    is(Object.keys(data[storageType].hosts).length,
       Object.keys(beforeReload[storageType]).length,
       "Number of hosts for " + storageType + "match");
    for (let host in beforeReload[storageType]) {
      ok(data[storageType].hosts[host], "Host " + host + " is present");
    }
  }
}

function markOutMatched(toBeEmptied, data, deleted) {
  if (!Object.keys(toBeEmptied).length) {
    info("Object empty");
    return;
  }
  ok(Object.keys(data).length,
     "At least one storage type should be present");
  for (let storageType in toBeEmptied) {
    if (!data[storageType]) {
      continue;
    }
    info("Testing for " + storageType);
    for (let host in data[storageType]) {
      ok(toBeEmptied[storageType][host], "Host " + host + " found");

      if (!deleted) {
        for (let item of data[storageType][host]) {
          let index = toBeEmptied[storageType][host].indexOf(item);
          ok(index > -1, "Item found - " + item);
          if (index > -1) {
            toBeEmptied[storageType][host].splice(index, 1);
          }
        }
        if (!toBeEmptied[storageType][host].length) {
          delete toBeEmptied[storageType][host];
        }
      } else {
        delete toBeEmptied[storageType][host];
      }
    }
    if (!Object.keys(toBeEmptied[storageType]).length) {
      delete toBeEmptied[storageType];
    }
  }
}

function testAddIframe(front) {
  info("Testing if new iframe addition works properly");
  return new Promise(resolve => {
    let shouldBeEmpty = {
      localStorage: {
        "https://sectest1.example.org": ["iframe-s-ls1"]
      },
      sessionStorage: {
        "https://sectest1.example.org": ["iframe-s-ss1"]
      },
      cookies: {
        "sectest1.example.org": [
          getCookieId("sc1", "sectest1.example.org",
                      "/browser/devtools/server/tests/browser/")
        ]
      },
      indexedDB: {
        // empty because indexed db creation happens after the page load, so at
        // the time of window-ready, there was no indexed db present.
        "https://sectest1.example.org": []
      },
      Cache: {
        "https://sectest1.example.org": []
      }
    };

    let onStoresUpdate = data => {
      info("checking if the hosts list is correct for this iframe addition");

      markOutMatched(shouldBeEmpty, data.added);

      ok(!data.changed || !data.changed.cookies ||
         !data.changed.cookies["https://sectest1.example.org"],
         "Nothing got changed for cookies");
      ok(!data.changed || !data.changed.localStorage ||
         !data.changed.localStorage["https://sectest1.example.org"],
         "Nothing got changed for local storage");
      ok(!data.changed || !data.changed.sessionStorage ||
         !data.changed.sessionStorage["https://sectest1.example.org"],
         "Nothing got changed for session storage");
      ok(!data.changed || !data.changed.indexedDB ||
         !data.changed.indexedDB["https://sectest1.example.org"],
         "Nothing got changed for indexed db");

      ok(!data.deleted || !data.deleted.cookies ||
         !data.deleted.cookies["https://sectest1.example.org"],
         "Nothing got deleted for cookies");
      ok(!data.deleted || !data.deleted.localStorage ||
         !data.deleted.localStorage["https://sectest1.example.org"],
         "Nothing got deleted for local storage");
      ok(!data.deleted || !data.deleted.sessionStorage ||
         !data.deleted.sessionStorage["https://sectest1.example.org"],
         "Nothing got deleted for session storage");
      ok(!data.deleted || !data.deleted.indexedDB ||
         !data.deleted.indexedDB["https://sectest1.example.org"],
         "Nothing got deleted for indexed db");

      if (!Object.keys(shouldBeEmpty).length) {
        info("Everything to be received is received.");
        endTestReloaded();
      }
    };

    let endTestReloaded = () => {
      front.off("stores-update", onStoresUpdate);
      resolve();
    };

    front.on("stores-update", onStoresUpdate);

    let iframe = content.document.createElement("iframe");
    iframe.src = ALT_DOMAIN_SECURED + "storage-secured-iframe.html";
    content.document.querySelector("body").appendChild(iframe);
  });
}

function testRemoveIframe(front) {
  info("Testing if iframe removal works properly");
  return new Promise(resolve => {
    let shouldBeEmpty = {
      localStorage: {
        "http://sectest1.example.org": []
      },
      sessionStorage: {
        "http://sectest1.example.org": []
      },
      Cache: {
        "http://sectest1.example.org": []
      },
      indexedDB: {
        "http://sectest1.example.org": []
      }
    };

    let onStoresUpdate = data => {
      info("checking if the hosts list is correct for this iframe deletion");

      markOutMatched(shouldBeEmpty, data.deleted, true);

      ok(!data.deleted.cookies || !data.deleted.cookies["sectest1.example.org"],
        "Nothing got deleted for Cookies as " +
        "the same hostname is still present");

      ok(!data.changed || !data.changed.cookies ||
         !data.changed.cookies["http://sectest1.example.org"],
         "Nothing got changed for cookies");
      ok(!data.changed || !data.changed.localStorage ||
         !data.changed.localStorage["http://sectest1.example.org"],
         "Nothing got changed for local storage");
      ok(!data.changed || !data.changed.sessionStorage ||
         !data.changed.sessionStorage["http://sectest1.example.org"],
         "Nothing got changed for session storage");

      ok(!data.added || !data.added.cookies ||
         !data.added.cookies["http://sectest1.example.org"],
         "Nothing got added for cookies");
      ok(!data.added || !data.added.localStorage ||
         !data.added.localStorage["http://sectest1.example.org"],
         "Nothing got added for local storage");
      ok(!data.added || !data.added.sessionStorage ||
         !data.added.sessionStorage["http://sectest1.example.org"],
         "Nothing got added for session storage");

      if (!Object.keys(shouldBeEmpty).length) {
        info("Everything to be received is received.");
        endTestReloaded();
      }
    };

    let endTestReloaded = () => {
      front.off("stores-update", onStoresUpdate);
      resolve();
    };

    front.on("stores-update", onStoresUpdate);

    for (let iframe of content.document.querySelectorAll("iframe")) {
      if (iframe.src.startsWith("http:")) {
        iframe.remove();
        break;
      }
    }
  });
}

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-dynamic-windows.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let front = StorageFront(client, form);
  let data = yield front.listStores();
  yield testStores(data, front);

  yield clearStorage();

  // Forcing GC/CC to get rid of docshells and windows created by this test.
  forceCollections();
  yield client.close();
  forceCollections();
  DebuggerServer.destroy();
  forceCollections();
});
