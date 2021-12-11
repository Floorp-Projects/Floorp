/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the storage panel is able to display multiple cookies with the same
// name (and different paths).

/* import-globals-from storage-helpers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/server/tests/browser/storage-helpers.js",
  this
);

const TESTDATA = {
  "http://test1.example.org": [
    {
      name: "name",
      value: "value1",
      expires: 0,
      path: "/",
      host: "test1.example.org",
      hostOnly: true,
      isSecure: false,
    },
    {
      name: "name",
      value: "value2",
      expires: 0,
      path: "/path2/",
      host: "test1.example.org",
      hostOnly: true,
      isSecure: false,
    },
    {
      name: "name",
      value: "value3",
      expires: 0,
      path: "/path3/",
      host: "test1.example.org",
      hostOnly: true,
      isSecure: false,
    },
  ],
};

add_task(async function() {
  const { commands } = await openTabAndSetupStorage(
    MAIN_DOMAIN + "storage-cookies-same-name.html"
  );

  const { resourceCommand } = commands;
  const { TYPES } = resourceCommand;
  const data = {};
  await resourceCommand.watchResources(
    [TYPES.COOKIE, TYPES.LOCAL_STORAGE, TYPES.SESSION_STORAGE],
    {
      async onAvailable(resources) {
        for (const resource of resources) {
          const { resourceType } = resource;
          if (!data[resourceType]) {
            data[resourceType] = { hosts: {}, dataByHost: {} };
          }

          for (const host in resource.hosts) {
            if (!data[resourceType].hosts[host]) {
              data[resourceType].hosts[host] = [];
            }
            // For indexed DB, we have some values, the database names. Other are empty arrays.
            const hostValues = resource.hosts[host];
            data[resourceType].hosts[host].push(...hostValues);
            data[resourceType].dataByHost[
              host
            ] = await resource.getStoreObjects(host);
          }
        }
      },
    }
  );

  ok(data.cookies, "Cookies storage actor is present");

  await testCookies(data.cookies);
  await clearStorage();

  // Forcing GC/CC to get rid of docshells and windows created by this test.
  forceCollections();
  await commands.destroy();
  forceCollections();
});

function testCookies({ hosts, dataByHost }) {
  const numHosts = Object.keys(hosts).length;
  is(numHosts, 1, "Correct number of host entries for cookies");
  return testCookiesObjects(0, hosts, dataByHost);
}

var testCookiesObjects = async function(index, hosts, dataByHost) {
  const host = Object.keys(hosts)[index];
  const data = dataByHost[host];
  is(
    data.total,
    TESTDATA[host].length,
    "Number of cookies in host " + host + " matches"
  );
  for (const item of data.data) {
    let found = false;
    for (const toMatch of TESTDATA[host]) {
      if (
        item.name === toMatch.name &&
        item.host === toMatch.host &&
        item.path === toMatch.path
      ) {
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

  ok(!!TESTDATA[host], "Host is present in the list : " + host);
  if (index == Object.keys(hosts).length - 1) {
    return;
  }
  await testCookiesObjects(++index, hosts, dataByHost);
};
