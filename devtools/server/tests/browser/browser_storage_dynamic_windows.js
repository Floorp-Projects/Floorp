/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { StorageFront } = require("devtools/shared/fronts/storage");
/* import-globals-from storage-helpers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/server/tests/browser/storage-helpers.js",
  this);

// beforeReload references an object representing the initialized state of the
// storage actor.
const beforeReload = {
  cookies: {
    "http://test1.example.org": ["c1", "cs2", "c3", "uc1"],
    "http://sectest1.example.org": ["uc1", "cs2"]
  },
  indexedDB: {
    "http://test1.example.org": [
      JSON.stringify(["idb1", "obj1"]),
      JSON.stringify(["idb1", "obj2"]),
      JSON.stringify(["idb2", "obj3"]),
    ],
    "http://sectest1.example.org": []
  },
  localStorage: {
    "http://test1.example.org": ["ls1", "ls2"],
    "http://sectest1.example.org": ["iframe-u-ls1"]
  },
  sessionStorage: {
    "http://test1.example.org": ["ss1"],
    "http://sectest1.example.org": ["iframe-u-ss1", "iframe-u-ss2"]
  }
};

// afterIframeAdded references the items added when an iframe containing storage
// items is added to the page.
const afterIframeAdded = {
  cookies: {
    "https://sectest1.example.org": [
      getCookieId("cs2", ".example.org", "/"),
      getCookieId("sc1", "sectest1.example.org",
                  "/browser/devtools/server/tests/browser/")
    ],
    "http://sectest1.example.org": [
      getCookieId("sc1", "sectest1.example.org",
                  "/browser/devtools/server/tests/browser/")
    ]
  },
  indexedDB: {
    // empty because indexed db creation happens after the page load, so at
    // the time of window-ready, there was no indexed db present.
    "https://sectest1.example.org": []
  },
  localStorage: {
    "https://sectest1.example.org": ["iframe-s-ls1"]
  },
  sessionStorage: {
    "https://sectest1.example.org": ["iframe-s-ss1"]
  }
};

// afterIframeRemoved references the items deleted when an iframe containing
// storage items is removed from the page.
const afterIframeRemoved = {
  cookies: {
    "http://sectest1.example.org": []
  },
  indexedDB: {
    "http://sectest1.example.org": []
  },
  localStorage: {
    "http://sectest1.example.org": []
  },
  sessionStorage: {
    "http://sectest1.example.org": []
  },
};

add_task(async function() {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-dynamic-windows.html");

  initDebuggerServer();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);
  const front = StorageFront(client, form);
  const data = await front.listStores();

  await testStores(data, front);

  await clearStorage();

  // Forcing GC/CC to get rid of docshells and windows created by this test.
  forceCollections();
  await client.close();
  forceCollections();
  DebuggerServer.destroy();
  forceCollections();
});

async function testStores(data, front) {
  testWindowsBeforeReload(data);

  await testAddIframe(front);
  await testRemoveIframe(front);
}

function testWindowsBeforeReload(data) {
  for (const storageType in beforeReload) {
    ok(data[storageType], `${storageType} storage actor is present`);
    is(Object.keys(data[storageType].hosts).length,
       Object.keys(beforeReload[storageType]).length,
        `Number of hosts for ${storageType} match`);
    for (const host in beforeReload[storageType]) {
      ok(data[storageType].hosts[host], `Host ${host} is present`);
    }
  }
}

async function testAddIframe(front) {
  info("Testing if new iframe addition works properly");

  const update = front.once("stores-update");

  await ContentTask.spawn(gBrowser.selectedBrowser, ALT_DOMAIN_SECURED,
    secured => {
      const doc = content.document;

      const iframe = doc.createElement("iframe");
      iframe.src = secured + "storage-secured-iframe.html";

      doc.querySelector("body").appendChild(iframe);
    }
  );

  const data = await update;

  validateStorage(data, afterIframeAdded, "added");
}

async function testRemoveIframe(front) {
  info("Testing if iframe removal works properly");

  const update = front.once("stores-update");

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, () => {
    for (const iframe of content.document.querySelectorAll("iframe")) {
      if (iframe.src.startsWith("http:")) {
        iframe.remove();
        break;
      }
    }
  });

  const data = await update;

  validateStorage(data, afterIframeRemoved, "deleted");
}

function validateStorage(actual, expected, category = "") {
  if (category) {
    for (const cat of ["added", "changed", "deleted"]) {
      if (cat === category) {
        ok(actual[cat], `Data from the iframe has been ${cat}.`);
      } else {
        ok(!actual[cat], `No data was ${cat}.`);
      }
    }
  }

  for (const [type, expectedData] of Object.entries(expected)) {
    const actualData = category ? actual[category][type] : actual[type];

    ok(actualData, `${type} contains data.`);

    const actualKeys = Object.keys(actualData);
    const expectedKeys = Object.keys(expectedData);
    is(actualKeys.length, expectedKeys.length, `${type} data is the correct length.`);

    for (const [key, dataValues] of Object.entries(expectedData)) {
      ok(actualData[key], `${type} data contains the key ${key}.`);

      for (const dataValue of dataValues) {
        ok(actualData[key].includes(dataValue),
          `${type}[${key}] contains "${dataValue}".`);
      }
    }
  }
}
