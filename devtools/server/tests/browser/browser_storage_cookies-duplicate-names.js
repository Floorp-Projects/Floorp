/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the storage panel is able to display multiple cookies with the same
// name (and different paths).

const {StorageFront} = require("devtools/shared/fronts/storage");
/* import-globals-from storage-helpers.js */
Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/devtools/server/tests/browser/storage-helpers.js", this);

const TESTDATA = {
  "http://test1.example.org": [
    {
      name: "name",
      value: "value1",
      expires: 0,
      path: "/",
      host: "test1.example.org",
      isDomain: false,
      isSecure: false,
    },
    {
      name: "name",
      value: "value2",
      expires: 0,
      path: "/path2/",
      host: "test1.example.org",
      isDomain: false,
      isSecure: false,
    },
    {
      name: "name",
      value: "value3",
      expires: 0,
      path: "/path3/",
      host: "test1.example.org",
      isDomain: false,
      isSecure: false,
    }
  ]
};

add_task(async function() {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-cookies-same-name.html");

  initDebuggerServer();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);
  const front = StorageFront(client, form);
  const data = await front.listStores();

  ok(data.cookies, "Cookies storage actor is present");

  await testCookies(data.cookies);
  await clearStorage();

  // Forcing GC/CC to get rid of docshells and windows created by this test.
  forceCollections();
  await client.close();
  forceCollections();
  DebuggerServer.destroy();
  forceCollections();
});

function testCookies(cookiesActor) {
  const numHosts = Object.keys(cookiesActor.hosts).length;
  is(numHosts, 1, "Correct number of host entries for cookies");
  return testCookiesObjects(0, cookiesActor.hosts, cookiesActor);
}

var testCookiesObjects = async function(index, hosts, cookiesActor) {
  const host = Object.keys(hosts)[index];
  const matchItems = data => {
    is(data.total, TESTDATA[host].length,
       "Number of cookies in host " + host + " matches");
    for (const item of data.data) {
      let found = false;
      for (const toMatch of TESTDATA[host]) {
        if (item.name === toMatch.name &&
            item.host === toMatch.host &&
            item.path === toMatch.path) {
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

  ok(!!TESTDATA[host], "Host is present in the list : " + host);
  matchItems(await cookiesActor.getStoreObjects(host));
  if (index == Object.keys(hosts).length - 1) {
    return;
  }
  await testCookiesObjects(++index, hosts, cookiesActor);
};
