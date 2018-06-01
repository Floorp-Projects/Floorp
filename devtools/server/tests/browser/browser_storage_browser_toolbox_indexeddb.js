/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from storage-helpers.js */

"use strict";

const { StorageFront } = require("devtools/shared/fronts/storage");
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/server/tests/browser/storage-helpers.js",
  this);

add_task(async function() {
  await pushPref("devtools.chrome.enabled", true);
  await pushPref("devtools.debugger.remote-enabled", true);
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-updates.html");
  await createIndexedDB();

  initDebuggerServer();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);
  const front = StorageFront(client, form);

  await testInternalDBs(front);
  await clearStorage();

  // Forcing GC/CC to get rid of docshells and windows created by this test.
  forceCollections();
  await client.close();
  forceCollections();
  DebuggerServer.destroy();
  forceCollections();
});

async function createIndexedDB() {
  const request = indexedDB.open("MyDatabase", 1);

  request.onupgradeneeded = function() {
    const db = request.result;
    const store = db.createObjectStore("MyObjectStore", {keyPath: "id"});
    store.createIndex("NameIndex", ["name.last", "name.first"]);
  };

  await undefined;
}

async function testInternalDBs(front) {
  const data = await front.listStores();
  const hosts = data.indexedDB.hosts;

  ok(hosts.chrome, `indexedDB hosts contains "chrome"`);

  const path = `["MyDatabase (persistent)","MyObjectStore"]`;
  const foundDB = hosts.chrome.includes(path);
  ok(foundDB, `Host "chrome" includes ${path}`);
}
