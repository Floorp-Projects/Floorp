// SPDX-License-Identifier: MPL-2.0
// Native transport proof; all accounts and encryption keys below are local fixtures.
// Evaluated by the isolated native migration runner.
// deno-lint-ignore no-unused-vars
async function runScenario() {
  const base = "resource://floorp-migration-validation/";
  const { WorkspaceContainerMigration: migration } = ChromeUtils.importESModule(
    base + "migration.sys.mjs",
  );
  const { updateWorkspaceSyncDocument } = ChromeUtils.importESModule(
    base + "model.sys.mjs",
  );
  const { workspaceMetadataRecord, workspaceTabRecord } = ChromeUtils
    .importESModule(base + "record.sys.mjs");
  const { Collection, CryptoWrapper, WBORecord } = ChromeUtils.importESModule(
    "resource://services-sync/record.sys.mjs",
  );
  const { Resource } = ChromeUtils.importESModule(
    "resource://services-sync/resource.sys.mjs",
  );
  const { BulkKeyBundle } = ChromeUtils.importESModule(
    "resource://services-sync/keys.sys.mjs",
  );
  const { Service } = ChromeUtils.importESModule(
    "resource://services-sync/service.sys.mjs",
  );
  const { Utils } = ChromeUtils.importESModule(
    "resource://services-sync/util.sys.mjs",
  );
  const { Log } = ChromeUtils.importESModule(
    "resource://gre/modules/Log.sys.mjs",
  );
  const log = Log.repository.getLogger("Sync.Test.WorkspaceMigration");
  const key = new BulkKeyBundle("migration-test");
  await key.generateRandom();
  class Record extends CryptoWrapper {}
  Utils.deferGetSet(Record, "cleartext", ["entity"]);
  const assert = (value, message) => {
    if (!value) throw new Error(message);
  };
  const cases = [];
  const serviceFor = (name) => {
    const cache = new Map();
    const service = {
      storageURL: `__MIGRATION_TEST_ORIGIN__/${name}/storage/`,
      metaURL: `__MIGRATION_TEST_ORIGIN__/${name}/storage/meta/global`,
      collectionKeys: { keyForCollection: () => key },
      resource: (url) => new Resource(url),
      _log: log,
      recordManager: {
        async get(url) {
          if (!cache.has(url)) {
            const record = await new WBORecord("meta", "global").fetch(
              new Resource(url),
            );
            if (!record.response.success) {
              throw new Error("Metadata GET failed");
            }
            cache.set(url, record);
          }
          return cache.get(url);
        },
        set: (url, record) => cache.set(url, record),
      },
      uploadMetaGlobal(meta) {
        return Service.uploadMetaGlobal.call(service, meta);
      },
    };
    return service;
  };
  const upload = async (service, collectionName, values, modified = 0) => {
    const collection = new Collection(
      service.storageURL + collectionName,
      Record,
      service,
    );
    let response;
    const queue = collection.newPostQueue(log, modified, (_queue, result) => {
      response = result;
    });
    for (const value of values) {
      const record = new Record(collectionName, value.id);
      if (value.deleted) record.deleted = true;
      else record.entity = value.entity;
      await record.encrypt(key);
      assert(
        (await queue.enqueue(record)).enqueued,
        "Native queue rejected fixture",
      );
    }
    await queue.flush(true);
    return response;
  };
  const read = async (service, name) => {
    const collection = new Collection(
      service.storageURL + name,
      Record,
      service,
    );
    collection.full = true;
    const result = await collection.getBatched(100);
    assert(result.response.success, "Native collection GET failed");
    for (const record of result.records) await record.decrypt(key);
    return result;
  };
  const deviceId = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
  const otherDevice = "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb";
  const workspaceId = "11111111-1111-4111-8111-111111111111";
  const tabId = "22222222-2222-4222-8222-222222222222";
  const retiredId = "33333333-3333-4333-8333-333333333333";
  const options = (accountUid, override = {}) => ({
    accountUid,
    deviceId,
    localDocuments: [],
    localRetired: [],
    containerDefinitions: [],
    assertCurrent() {},
    ...override,
  });
  const service = serviceFor("main");
  const doc = updateWorkspaceSyncDocument(
    null,
    {
      workspaceId,
      name: "Legacy",
      icon: null,
      tabs: [{
        id: tabId,
        url: "https://example.test/",
        title: "Legacy tab",
        pinned: false,
      }],
    },
    deviceId,
    10,
  );
  const source = [{ id: workspaceId, entity: workspaceMetadataRecord(doc) }, {
    id: tabId,
    entity: workspaceTabRecord(workspaceId, doc.tabs[0]),
  }, { id: retiredId, deleted: true }];
  assert(
    (await upload(service, "floorp-workspaces", source)).success,
    "Seed failed",
  );
  const before = JSON.stringify(
    (await read(service, "floorp-workspaces")).records.map((record) =>
      record.cleartext
    ),
  );
  const stage = await migration.prepare(service, options("main"));
  assert(
    stage.phase === "pending" && stage.pendingIds.length === 3,
    "Did not backfill all legacy records",
  );
  assert(
    stage.snapshot.documents[0].tabs[0].container.kind === "legacy-unresolved",
    "Legacy ref guessed none",
  );
  const meta = await service.recordManager.get(service.metaURL);
  assert(
    meta.payload.engines["floorp-workspaces-v2"].version === 1 &&
      meta.payload.engines.bookmarks.syncID === "retain-bookmarks",
    "Native metadata preflight damaged engine metadata",
  );
  cases.push(
    "native encrypted paged backfill preserves tombstones and metadata",
  );

  const { WorkspaceContainerMigration: restart } = ChromeUtils.importESModule(
    base + "restart.sys.mjs",
  );
  const resumed = await restart.prepare(service, options("main"));
  assert(
    resumed.migrationId === stage.migrationId &&
      resumed.pendingIds.length === 3,
    "Durable journal restart lost pending writes",
  );
  cases.push("durable journal resumes interrupted owner");
  const exact = restart.pendingRecords();
  const wrong = structuredClone(exact);
  wrong.find((record) => record.id === tabId).entity.tab.title =
    "Different edit";
  const partial = await restart.acknowledge(service, wrong, () => {});
  assert(
    partial.phase === "pending" && partial.pendingIds.length === 1,
    "ACK accepted changed manifest",
  );
  let missingRejected = false;
  try {
    await restart.acknowledge(service, exact, () => {});
  } catch (error) {
    missingRejected = error.code === "write-not-acknowledged";
  }
  assert(missingRejected, "Completion did not verify server writes");
  assert(
    (await upload(service, "floorp-workspaces-v2", exact)).success,
    "Native initial upload failed",
  );
  const completed = await restart.acknowledge(service, exact, () => {});
  assert(completed.phase === "complete", "Completion marker missing");
  const after = JSON.stringify(
    (await read(service, "floorp-workspaces")).records.map((record) =>
      record.cleartext
    ),
  );
  assert(before === after, "Legacy collection changed");
  cases.push("exact ACK and server manifest gate completion; v1 unchanged");
  // The first module still has its own pending journal in memory. It must obey the account marker.
  const completedControl = await read(service, "floorp-workspaces-migration");
  const completeRecord = completedControl.records[0];
  const foreignMigrationId = "44444444-4444-4444-8444-444444444444";
  assert(
    (await upload(service, "floorp-workspaces-migration", [{
      id: completeRecord.id,
      entity: {
        ...completeRecord.entity,
        migrationId: foreignMigrationId,
        ownerId: otherDevice,
      },
    }], Number(completedControl.response.headers["x-last-modified"]))).success,
    "Could not create foreign complete fixture",
  );
  const fromOtherDevice = await migration.prepare(
    service,
    options("main", { deviceId: otherDevice }),
  );
  assert(
    fromOtherDevice.phase === "complete" &&
      fromOtherDevice.migrationId === foreignMigrationId &&
      migration.pendingRecords().length === 0,
    "Stale pending manifest exposed after completion",
  );
  cases.push(
    "completed account marker prevents legacy reimport and stale pending writes",
  );

  await migration.resetAccount();
  const pendingService = serviceFor("pending");
  const pending = await migration.prepare(
    pendingService,
    options("pending", { localDocuments: [doc] }),
  );
  assert(pending.phase === "pending", "Pending fixture unexpectedly completed");
  let blocked = false;
  try {
    await migration.prepare(
      pendingService,
      options("pending", { deviceId: otherDevice }),
    );
  } catch (error) {
    blocked = error.code === "other-owner";
  }
  assert(
    blocked && migration.pendingRecords().length === 0,
    "Foreign owner could upload pending records",
  );
  await migration.resetAccount();
  let missingJournal = false;
  try {
    await migration.prepare(pendingService, options("pending"));
  } catch (error) {
    missingJournal = error.code === "journal-missing";
  }
  assert(missingJournal, "Missing owner journal did not fail closed");
  cases.push("other owner and lost journal are blocked without lock stealing");

  const raceService = serviceFor("race");
  const control =
    (await read(pendingService, "floorp-workspaces-migration")).records[0];
  const firstClaim = await upload(raceService, "floorp-workspaces-migration", [{
    id: control.id,
    entity: control.entity,
  }], 0);
  const secondClaim = await upload(raceService, "floorp-workspaces-migration", [
    { id: control.id, entity: { ...control.entity, ownerId: otherDevice } },
  ], 0);
  assert(
    firstClaim.success && secondClaim.status === 412,
    "Concurrent native CAS claim overwrote owner",
  );
  cases.push("native PostQueue CAS rejects a simultaneous second claim");

  await migration.resetAccount();
  const empty = await migration.prepare(serviceFor("empty"), options("empty"));
  assert(
    empty.phase === "complete",
    "Empty account migration did not complete",
  );
  cases.push("empty account initializes without legacy writes");
  await migration.resetAccount();
  const inconsistent = serviceFor("inconsistent");
  await upload(inconsistent, "floorp-workspaces-v2", exact);
  let inconsistentBlocked = false;
  try {
    await migration.prepare(inconsistent, options("inconsistent"));
  } catch (error) {
    inconsistentBlocked = error.code === "inconsistent-server";
  }
  assert(inconsistentBlocked, "Existing v2 data without marker could be wiped");
  cases.push("preexisting v2 without metadata/claim fails closed");
  await migration.resetAccount();
  // Load this account with no journal first, so the later directory collision
  // fails the atomic write after staging, rather than failing journal loading.
  try {
    await migration.prepare(inconsistent, options("durable-retry"));
  } catch (error) {
    assert(
      error.code === "inconsistent-server",
      "Could not prime durable retry account",
    );
  }
  const journalPath = PathUtils.join(
    PathUtils.profileDir,
    "floorp-workspaces-migration.json",
  );
  await IOUtils.remove(journalPath);
  await IOUtils.makeDirectory(journalPath);
  const durableService = serviceFor("durable-retry");
  let writeFailed = false;
  try {
    await migration.prepare(
      durableService,
      options("durable-retry", { localDocuments: [doc] }),
    );
  } catch {
    writeFailed = true;
  }
  assert(
    writeFailed && migration.pendingRecords().length === 0,
    "Failed durable journal write exposed claim",
  );
  await IOUtils.remove(journalPath);
  const retried = await migration.prepare(
    durableService,
    options("durable-retry", { localDocuments: [doc] }),
  );
  assert(retried.phase === "pending", "Journal write retry failed");
  const durable = await IOUtils.readJSON(journalPath);
  assert(
    durable.control.migrationId === retried.migrationId &&
      durable.pendingIds.length === 2,
    "Retry claimed without durable snapshot",
  );
  cases.push("failed durable journal write is retried before acquiring claim");
  await ChromeUtils.importESModule(base + "migration-test.sys.mjs")
    .runAllTests();
  return { passed: true, cases, pureMigrationTests: 8 };
}
