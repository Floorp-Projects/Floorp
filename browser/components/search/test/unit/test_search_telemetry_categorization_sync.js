/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the integration of Remote Settings with SERP domain categorization.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  SearchSERPDomainToCategoriesMap:
    "resource:///modules/SearchSERPTelemetry.sys.mjs",
  TELEMETRY_CATEGORIZATION_KEY:
    "resource:///modules/SearchSERPTelemetry.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
});

async function waitForDomainToCategoriesUpdate() {
  return TestUtils.topicObserved("domain-to-categories-map-update-complete");
}

async function mockRecordWithCachedAttachment({ id, version, filename }) {
  // Get the bytes of the file for the hash and size for attachment metadata.
  let data = await IOUtils.readUTF8(
    PathUtils.join(do_get_cwd().path, filename)
  );
  let buffer = new TextEncoder().encode(data).buffer;
  let stream = Cc["@mozilla.org/io/arraybuffer-input-stream;1"].createInstance(
    Ci.nsIArrayBufferInputStream
  );
  stream.setData(buffer, 0, buffer.byteLength);

  // Generate a hash.
  let hasher = Cc["@mozilla.org/security/hash;1"].createInstance(
    Ci.nsICryptoHash
  );
  hasher.init(Ci.nsICryptoHash.SHA256);
  hasher.updateFromStream(stream, -1);
  let hash = hasher.finish(false);
  hash = Array.from(hash, (_, i) =>
    ("0" + hash.charCodeAt(i).toString(16)).slice(-2)
  ).join("");

  let record = {
    id,
    version,
    attachment: {
      hash,
      location: `main-workspace/search-categorization/${filename}`,
      filename,
      size: buffer.byteLength,
      mimetype: "application/json",
    },
  };

  client.attachments.cacheImpl.set(id, {
    record,
    blob: new Blob([buffer]),
  });

  return record;
}

const RECORD_A_ID = Services.uuid.generateUUID().number.slice(1, -1);
const RECORD_B_ID = Services.uuid.generateUUID().number.slice(1, -1);

const client = RemoteSettings(TELEMETRY_CATEGORIZATION_KEY);
const db = client.db;

const RECORDS = {
  record1a: {
    id: RECORD_A_ID,
    version: 1,
    filename: "domain_category_mappings_1a.json",
  },
  record1b: {
    id: RECORD_B_ID,
    version: 1,
    filename: "domain_category_mappings_1b.json",
  },
  record2a: {
    id: RECORD_A_ID,
    version: 2,
    filename: "domain_category_mappings_2a.json",
  },
  record2b: {
    id: RECORD_B_ID,
    version: 2,
    filename: "domain_category_mappings_2b.json",
  },
};

add_setup(async () => {
  // Testing with Remote Settings requires a profile.
  do_get_profile();
  await db.clear();
});

add_task(async function test_initial_import() {
  info("Create record containing domain_category_mappings_1a.json attachment.");
  let record1a = await mockRecordWithCachedAttachment(RECORDS.record1a);
  await db.create(record1a);

  info("Create record containing domain_category_mappings_1b.json attachment.");
  let record1b = await mockRecordWithCachedAttachment(RECORDS.record1b);
  await db.create(record1b);

  info("Add data to Remote Settings DB.");
  await db.importChanges({}, Date.now());

  info("Initialize search categorization mappings.");
  let promise = waitForDomainToCategoriesUpdate();
  await SearchSERPDomainToCategoriesMap.init();
  await promise;

  Assert.deepEqual(
    SearchSERPDomainToCategoriesMap.get("example.com"),
    [{ category: 1, score: 100 }],
    "Return value from lookup of example.com should be the same."
  );

  Assert.deepEqual(
    SearchSERPDomainToCategoriesMap.get("example.org"),
    [{ category: 2, score: 90 }],
    "Return value from lookup of example.org should be the same."
  );

  // Clean up.
  await db.clear();
  SearchSERPDomainToCategoriesMap.uninit();
});

add_task(async function test_update_records() {
  info("Create record containing domain_category_mappings_1a.json attachment.");
  let record1a = await mockRecordWithCachedAttachment(RECORDS.record1a);
  await db.create(record1a);

  info("Create record containing domain_category_mappings_1b.json attachment.");
  let record1b = await mockRecordWithCachedAttachment(RECORDS.record1b);
  await db.create(record1b);

  info("Add data to Remote Settings DB.");
  await db.importChanges({}, Date.now());

  info("Initialize search categorization mappings.");
  let promise = waitForDomainToCategoriesUpdate();
  await SearchSERPDomainToCategoriesMap.init();
  await promise;

  info("Send update from Remote Settings with updates to attachments.");
  let record2a = await mockRecordWithCachedAttachment(RECORDS.record2a);
  let record2b = await mockRecordWithCachedAttachment(RECORDS.record2b);
  const payload = {
    current: [record2a, record2b],
    created: [],
    updated: [
      { old: record1a, new: record2a },
      { old: record1b, new: record2b },
    ],
    deleted: [],
  };
  promise = waitForDomainToCategoriesUpdate();
  await client.emit("sync", {
    data: payload,
  });
  await promise;

  Assert.deepEqual(
    SearchSERPDomainToCategoriesMap.get("example.com"),
    [{ category: 1, score: 80 }],
    "Return value from lookup of example.com should have changed."
  );

  Assert.deepEqual(
    SearchSERPDomainToCategoriesMap.get("example.org"),
    [
      { category: 2, score: 50 },
      { category: 4, score: 80 },
    ],
    "Return value from lookup of example.org should have changed."
  );

  Assert.equal(
    SearchSERPDomainToCategoriesMap.version,
    2,
    "Version should be correct."
  );

  // Clean up.
  await db.clear();
  SearchSERPDomainToCategoriesMap.uninit();
});

add_task(async function test_delayed_initial_import() {
  info("Initialize search categorization mappings.");
  let observeNoRecordsFound = TestUtils.consoleMessageObserved(msg => {
    return (
      typeof msg.wrappedJSObject.arguments?.[0] == "string" &&
      msg.wrappedJSObject.arguments[0].includes(
        "No records found for domain-to-categories map."
      )
    );
  });
  info("Initialize without records.");
  await SearchSERPDomainToCategoriesMap.init();
  await observeNoRecordsFound;

  Assert.ok(SearchSERPDomainToCategoriesMap.empty, "Map is empty.");

  info("Send update from Remote Settings with updates to attachments.");
  let record1a = await mockRecordWithCachedAttachment(RECORDS.record1a);
  let record1b = await mockRecordWithCachedAttachment(RECORDS.record1b);
  const payload = {
    current: [record1a, record1b],
    created: [record1a, record1b],
    updated: [],
    deleted: [],
  };
  let promise = waitForDomainToCategoriesUpdate();
  await client.emit("sync", {
    data: payload,
  });
  await promise;

  Assert.deepEqual(
    SearchSERPDomainToCategoriesMap.get("example.com"),
    [{ category: 1, score: 100 }],
    "Return value from lookup of example.com should be the same."
  );

  Assert.deepEqual(
    SearchSERPDomainToCategoriesMap.get("example.org"),
    [{ category: 2, score: 90 }],
    "Return value from lookup of example.org should be the same."
  );

  Assert.equal(
    SearchSERPDomainToCategoriesMap.version,
    1,
    "Version should be correct."
  );

  // Clean up.
  await db.clear();
  SearchSERPDomainToCategoriesMap.uninit();
});

add_task(async function test_remove_record() {
  info("Create record containing domain_category_mappings_2a.json attachment.");
  let record2a = await mockRecordWithCachedAttachment(RECORDS.record2a);
  await db.create(record2a);

  info("Create record containing domain_category_mappings_2b.json attachment.");
  let record2b = await mockRecordWithCachedAttachment(RECORDS.record2b);
  await db.create(record2b);

  info("Add data to Remote Settings DB.");
  await db.importChanges({}, Date.now());

  info("Initialize search categorization mappings.");
  let promise = waitForDomainToCategoriesUpdate();
  await SearchSERPDomainToCategoriesMap.init();
  await promise;

  Assert.deepEqual(
    SearchSERPDomainToCategoriesMap.get("example.com"),
    [{ category: 1, score: 80 }],
    "Initialized properly."
  );

  info("Send update from Remote Settings with one removed record.");
  const payload = {
    current: [record2a],
    created: [],
    updated: [],
    deleted: [record2b],
  };
  promise = waitForDomainToCategoriesUpdate();
  await client.emit("sync", {
    data: payload,
  });
  await promise;

  Assert.deepEqual(
    SearchSERPDomainToCategoriesMap.get("example.com"),
    [{ category: 1, score: 80 }],
    "Return value from lookup of example.com should remain unchanged."
  );

  Assert.deepEqual(
    SearchSERPDomainToCategoriesMap.get("example.org"),
    [],
    "Return value from lookup of example.org should be empty."
  );

  Assert.equal(
    SearchSERPDomainToCategoriesMap.version,
    2,
    "Version should be correct."
  );

  // Clean up.
  await db.clear();
  SearchSERPDomainToCategoriesMap.uninit();
});

add_task(async function test_different_versions_coexisting() {
  info("Create record containing domain_category_mappings_1a.json attachment.");
  let record1a = await mockRecordWithCachedAttachment(RECORDS.record1a);
  await db.create(record1a);

  info("Create record containing domain_category_mappings_2b.json attachment.");
  let record2b = await mockRecordWithCachedAttachment(RECORDS.record2b);
  await db.create(record2b);

  info("Add data to Remote Settings DB.");
  await db.importChanges({}, Date.now());

  info("Initialize search categorization mappings.");
  let promise = waitForDomainToCategoriesUpdate();
  await SearchSERPDomainToCategoriesMap.init();
  await promise;

  Assert.deepEqual(
    SearchSERPDomainToCategoriesMap.get("example.com"),
    [
      {
        category: 1,
        score: 100,
      },
    ],
    "Should have a record from an older version."
  );

  Assert.deepEqual(
    SearchSERPDomainToCategoriesMap.get("example.org"),
    [
      { category: 2, score: 50 },
      { category: 4, score: 80 },
    ],
    "Return value from lookup of example.org should have the most recent value."
  );

  Assert.equal(
    SearchSERPDomainToCategoriesMap.version,
    2,
    "Version should be the latest."
  );

  // Clean up.
  await db.clear();
  SearchSERPDomainToCategoriesMap.uninit();
});

add_task(async function test_download_error() {
  info("Create record containing domain_category_mappings_1a.json attachment.");
  let record1a = await mockRecordWithCachedAttachment(RECORDS.record1a);
  await db.create(record1a);

  info("Add data to Remote Settings DB.");
  await db.importChanges({}, Date.now());

  info("Initialize search categorization mappings.");
  let promise = waitForDomainToCategoriesUpdate();
  await SearchSERPDomainToCategoriesMap.init();
  await promise;

  Assert.deepEqual(
    SearchSERPDomainToCategoriesMap.get("example.com"),
    [
      {
        category: 1,
        score: 100,
      },
    ],
    "Domain should have an entry in the map."
  );

  Assert.equal(
    SearchSERPDomainToCategoriesMap.version,
    1,
    "Version should be present."
  );

  info("Delete attachment from local cache.");
  client.attachments.cacheImpl.delete(RECORD_A_ID);

  const payload = {
    current: [record1a],
    created: [],
    updated: [record1a],
    deleted: [],
  };

  info("Sync payload.");
  let observeDownloadError = TestUtils.consoleMessageObserved(msg => {
    return (
      typeof msg.wrappedJSObject.arguments?.[0] == "string" &&
      msg.wrappedJSObject.arguments[0].includes("Could not download file:")
    );
  });
  await client.emit("sync", {
    data: payload,
  });
  await observeDownloadError;

  Assert.deepEqual(
    SearchSERPDomainToCategoriesMap.get("example.com"),
    [],
    "Domain should not exist in store."
  );

  Assert.equal(
    SearchSERPDomainToCategoriesMap.version,
    null,
    "Version should remain null."
  );

  // Clean up.
  await db.clear();
  SearchSERPDomainToCategoriesMap.uninit();
});
