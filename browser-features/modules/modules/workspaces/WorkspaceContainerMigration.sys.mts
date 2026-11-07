// SPDX-License-Identifier: MPL-2.0

import {
  acknowledgeWorkspaceMigrationRecords,
  buildWorkspaceMigrationSnapshot,
  canonicalWorkspaceMigrationRecord,
  type WorkspaceMigrationInputRecord,
  type WorkspaceMigrationRecord,
  type WorkspaceMigrationSnapshot,
} from "../../common/workspace-container-migration.ts";
import {
  isWorkspaceSyncId,
  parseWorkspaceSyncDocument,
} from "../../common/workspace-sync.ts";
import { parseWorkspaceContainerDefinition } from "../../common/workspace-container.ts";
import type { WorkspaceSyncDocument } from "../../common/workspace-sync-types.ts";
import type { WorkspaceContainerDefinition } from "../../common/workspace-container-types.ts";

const { Collection, CryptoWrapper } = ChromeUtils.importESModule(
  "resource://services-sync/record.sys.mjs",
);
const { Utils } = ChromeUtils.importESModule(
  "resource://services-sync/util.sys.mjs",
);
const { Log } = ChromeUtils.importESModule(
  "resource://gre/modules/Log.sys.mjs",
);

const LEGACY_COLLECTION = "floorp-workspaces";
const V2_COLLECTION = "floorp-workspaces-v2";
const CONTROL_COLLECTION = "floorp-workspaces-migration";
const CONTROL_ID = "e8d1a249-86cb-4464-a898-901d2de7a937";
const JOURNAL_PATH = PathUtils.join(
  PathUtils.profileDir,
  "floorp-workspaces-migration.json",
);
const log = Log.repository.getLogger("Sync.Engine.FloorpWorkspaceMigration");

interface NativeResponse {
  success: boolean;
  status: number;
  obj: unknown;
  headers: Record<string, string>;
}

interface NativeMetadata {
  payload: {
    engines?: Record<string, { version?: number; syncID?: string }>;
    [key: string]: unknown;
  };
  modified: number;
}

/** The existing authenticated native Service, never an application HTTP client. */
export interface WorkspaceMigrationService {
  storageURL: string;
  metaURL: string;
  collectionKeys: { keyForCollection(name: string): unknown };
  recordManager: {
    get(url: string): Promise<NativeMetadata | null>;
  };
  uploadMetaGlobal(meta: NativeMetadata): Promise<void>;
}

export interface WorkspaceMigrationOptions {
  accountUid: string;
  deviceId: string;
  localDocuments: readonly WorkspaceSyncDocument[];
  localRetired: readonly string[];
  localMetadataKnown?: readonly string[];
  containerDefinitions: readonly WorkspaceContainerDefinition[];
  /** Must throw after sign-out, account changes or feature cancellation. */
  assertCurrent: () => void;
}

export type WorkspaceMigrationStatus =
  | { phase: "complete"; migrationId: string }
  | {
    phase: "pending";
    migrationId: string;
    snapshot: WorkspaceMigrationSnapshot;
    pendingIds: string[];
  };

type MigrationErrorCode =
  | "other-owner"
  | "journal-missing"
  | "inconsistent-server"
  | "invalid-record"
  | "network-failure"
  | "account-changed"
  | "write-not-acknowledged";

export class WorkspaceMigrationError extends Error {
  constructor(readonly code: MigrationErrorCode) {
    super(`Workspace container migration: ${code}`);
    this.name = "WorkspaceMigrationError";
  }
}

interface MigrationControl {
  version: 1;
  kind: "workspace-container-migration";
  migrationId: string;
  ownerId: string;
  phase: "pending" | "complete";
  sourceModified: number;
  manifestHash: string;
  recordCount: number;
}

interface MigrationJournal {
  version: 1;
  accountUid: string;
  deviceId: string;
  control: MigrationControl;
  snapshot: WorkspaceMigrationSnapshot;
  pendingIds: string[];
}

class MigrationRecord extends CryptoWrapper {
  declare entity: unknown;
  constructor(collection?: string, id?: string) {
    super(collection, id);
  }
}
Utils.deferGetSet(MigrationRecord, "cleartext", ["entity"]);

let journal: MigrationJournal | null = null;
let activeAccount: string | null = null;
let activePhase: "pending" | "complete" | null = null;
let serial = Promise.resolve();
let epoch = 0;

function object(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function parseControl(value: unknown): MigrationControl {
  if (
    !object(value) || value.version !== 1 ||
    value.kind !== "workspace-container-migration" ||
    !isWorkspaceSyncId(value.migrationId) ||
    !isWorkspaceSyncId(value.ownerId) ||
    (value.phase !== "pending" && value.phase !== "complete") ||
    typeof value.sourceModified !== "number" ||
    !Number.isFinite(value.sourceModified) ||
    value.sourceModified < 0 ||
    typeof value.manifestHash !== "string" ||
    !/^[0-9a-f]{64}$/.test(value.manifestHash) ||
    typeof value.recordCount !== "number" ||
    !Number.isSafeInteger(value.recordCount) ||
    value.recordCount < 0
  ) throw new WorkspaceMigrationError("invalid-record");
  return {
    version: 1,
    kind: "workspace-container-migration",
    migrationId: value.migrationId,
    ownerId: value.ownerId,
    phase: value.phase,
    sourceModified: value.sourceModified,
    manifestHash: value.manifestHash,
    recordCount: value.recordCount,
  };
}

function enqueue<T>(operation: () => Promise<T>): Promise<T> {
  const result = serial.then(operation);
  serial = result.then(() => {}, () => {});
  return result;
}

async function persist(expected: number): Promise<void> {
  if (expected !== epoch) throw new WorkspaceMigrationError("account-changed");
  // JSONFile.saveSoon/_save does not establish a durable successful write.
  await IOUtils.writeJSON(JOURNAL_PATH, journal, {
    tmpPath: `${JOURNAL_PATH}.tmp`,
  });
  if (expected !== epoch) throw new WorkspaceMigrationError("account-changed");
}

async function manifestHash(
  records: readonly WorkspaceMigrationRecord[],
): Promise<string> {
  const bytes = new TextEncoder().encode(
    records.map(canonicalWorkspaceMigrationRecord).join("\n"),
  );
  const digest = await crypto.subtle.digest(
    "SHA-256",
    new Uint8Array(bytes).buffer,
  );
  return [...new Uint8Array(digest)].map((v) => v.toString(16).padStart(2, "0"))
    .join("");
}

async function load(accountUid: string, expected: number): Promise<void> {
  if (activeAccount === accountUid) return;
  journal = null;
  const exists = await IOUtils.exists(JOURNAL_PATH);
  if (expected !== epoch) throw new WorkspaceMigrationError("account-changed");
  if (exists) {
    const raw = await IOUtils.readJSON(JOURNAL_PATH);
    if (expected !== epoch) {
      throw new WorkspaceMigrationError("account-changed");
    }
    if (raw !== null) {
      if (
        !object(raw) || raw.version !== 1 || typeof raw.accountUid !== "string"
      ) {
        throw new WorkspaceMigrationError("invalid-record");
      }
      if (raw.accountUid === accountUid) {
        if (
          !isWorkspaceSyncId(raw.deviceId) || !object(raw.snapshot) ||
          !Array.isArray(raw.snapshot.records) || !Array.isArray(raw.pendingIds)
        ) {
          throw new WorkspaceMigrationError("invalid-record");
        }
        const parsed = raw as unknown as MigrationJournal;
        parsed.control = parseControl(raw.control);
        if (
          !Array.isArray(parsed.snapshot.documents) ||
          !Array.isArray(parsed.snapshot.metadataKnown) ||
          !Array.isArray(parsed.snapshot.definitions) ||
          !Array.isArray(parsed.snapshot.retiredIds) ||
          parsed.snapshot.metadataKnown.some((id) => !isWorkspaceSyncId(id)) ||
          parsed.snapshot.retiredIds.some((id) => !isWorkspaceSyncId(id))
        ) {
          throw new WorkspaceMigrationError("invalid-record");
        }
        parsed.snapshot.documents = parsed.snapshot.documents.map((doc) =>
          parseWorkspaceSyncDocument(doc, 2)
        );
        parsed.snapshot.definitions = parsed.snapshot.definitions.map(
          parseWorkspaceContainerDefinition,
        );
        if (parsed.control.ownerId !== parsed.deviceId) {
          throw new WorkspaceMigrationError("invalid-record");
        }
        const records = parsed.snapshot.records;
        const ids = new Set(records.map((record) => {
          canonicalWorkspaceMigrationRecord(record);
          return record.id;
        }));
        if (
          ids.size !== records.length ||
          records.length !== parsed.control.recordCount ||
          parsed.pendingIds.some((id) => !ids.has(id)) ||
          await manifestHash(records) !== parsed.control.manifestHash
        ) {
          throw new WorkspaceMigrationError("invalid-record");
        }
        // The derived document cache must describe the same exact manifest.
        const rebuilt = buildWorkspaceMigrationSnapshot(
          [],
          parsed.snapshot.documents,
          parsed.snapshot.retiredIds,
          parsed.snapshot.definitions,
          parsed.snapshot.metadataKnown,
        );
        if (
          await manifestHash(rebuilt.records) !== parsed.control.manifestHash
        ) {
          throw new WorkspaceMigrationError("invalid-record");
        }
        journal = parsed;
      }
    }
  }
  activeAccount = accountUid;
}

async function readCollection(
  service: WorkspaceMigrationService,
  name: string,
  assertCurrent: () => void,
): Promise<{ records: WorkspaceMigrationInputRecord[]; modified: number }> {
  assertCurrent();
  const collection = new Collection(
    `${service.storageURL}${name}`,
    MigrationRecord,
    service,
  );
  collection.full = true;
  const { response, records } = await collection.getBatched(100);
  assertCurrent();
  if (response.status === 404) return { records: [], modified: 0 };
  if (!response.success) throw new WorkspaceMigrationError("network-failure");
  const result: WorkspaceMigrationInputRecord[] = [];
  const key = service.collectionKeys.keyForCollection(name);
  for (const record of records) {
    assertCurrent();
    await record.decrypt(key);
    assertCurrent();
    if (!isWorkspaceSyncId(record.id)) {
      throw new WorkspaceMigrationError("invalid-record");
    }
    if (
      (record.deleted !== undefined && record.deleted !== false &&
        record.deleted !== true) ||
      (record.deleted === true && record.entity !== undefined)
    ) {
      throw new WorkspaceMigrationError("invalid-record");
    }
    result.push(
      record.deleted === true
        ? { id: record.id, deleted: true }
        : { id: record.id, entity: record.entity },
    );
  }
  const modified = Number(response.headers["x-last-modified"]);
  if (!Number.isFinite(modified) || modified < 0) {
    throw new WorkspaceMigrationError("invalid-record");
  }
  return { records: result, modified };
}

async function readControl(
  service: WorkspaceMigrationService,
  assertCurrent: () => void,
) {
  const fetched = await readCollection(
    service,
    CONTROL_COLLECTION,
    assertCurrent,
  );
  if (fetched.records.length === 0) {
    return { control: null, modified: fetched.modified };
  }
  if (
    fetched.records.length !== 1 || fetched.records[0].id !== CONTROL_ID ||
    fetched.records[0].deleted
  ) {
    throw new WorkspaceMigrationError("inconsistent-server");
  }
  return {
    control: parseControl(fetched.records[0].entity),
    modified: fetched.modified,
  };
}

/** Native PostQueue supplies the collection-level conditional request header. */
async function writeControl(
  service: WorkspaceMigrationService,
  control: MigrationControl,
  modified: number,
  assertCurrent: () => void,
): Promise<void> {
  assertCurrent();
  const collection = new Collection(
    `${service.storageURL}${CONTROL_COLLECTION}`,
    MigrationRecord,
    service,
  );
  const record = new MigrationRecord(CONTROL_COLLECTION, CONTROL_ID);
  record.entity = control;
  await record.encrypt(
    service.collectionKeys.keyForCollection(CONTROL_COLLECTION),
  );
  assertCurrent();
  let accepted = false;
  const queue = collection.newPostQueue(log, modified, (
    _queue: unknown,
    response: NativeResponse,
    batchOngoing: boolean,
  ) => {
    assertCurrent();
    if (!response.success) {
      throw new WorkspaceMigrationError(
        response.status === 412 ? "other-owner" : "network-failure",
      );
    }
    if (
      !object(response.obj) || !Array.isArray(response.obj.success) ||
      !response.obj.success.includes(CONTROL_ID) ||
      (object(response.obj.failed) &&
        Object.keys(response.obj.failed).length > 0)
    ) {
      throw new WorkspaceMigrationError("write-not-acknowledged");
    }
    if (!batchOngoing) accepted = true;
  });
  const queued = await queue.enqueue(record);
  if (!queued.enqueued) {
    throw new WorkspaceMigrationError("write-not-acknowledged");
  }
  await queue.flush(true);
  assertCurrent();
  if (!accepted) throw new WorkspaceMigrationError("write-not-acknowledged");
}

async function ensureMetadata(
  service: WorkspaceMigrationService,
  mayCreate: boolean,
  assertCurrent: () => void,
): Promise<void> {
  const meta = await service.recordManager.get(service.metaURL);
  assertCurrent();
  if (!meta || !object(meta.payload)) {
    throw new WorkspaceMigrationError("inconsistent-server");
  }
  const engines = meta.payload.engines ?? {};
  const existing = engines[V2_COLLECTION];
  if (existing) {
    if (
      existing.version !== 1 || typeof existing.syncID !== "string" ||
      !existing.syncID
    ) {
      throw new WorkspaceMigrationError("inconsistent-server");
    }
    return;
  }
  if (!mayCreate) throw new WorkspaceMigrationError("inconsistent-server");
  const next = { version: 1, syncID: Utils.makeGUID() };
  engines[V2_COLLECTION] = next;
  meta.payload.engines = engines;
  try {
    // This native helper uses X-If-Unmodified-Since and refreshes recordManager.
    // It preserves the other engine entries, keys, and declined preferences.
    await service.uploadMetaGlobal(meta);
    assertCurrent();
  } catch (error) {
    if (engines[V2_COLLECTION] === next) delete engines[V2_COLLECTION];
    throw error;
  }
}

function status(): WorkspaceMigrationStatus {
  if (!journal) throw new WorkspaceMigrationError("journal-missing");
  if (journal.control.phase === "complete") {
    return { phase: "complete", migrationId: journal.control.migrationId };
  }
  return {
    phase: "pending",
    migrationId: journal.control.migrationId,
    snapshot: structuredClone(journal.snapshot),
    pendingIds: [...journal.pendingIds],
  };
}

async function finish(
  service: WorkspaceMigrationService,
  assertCurrent: () => void,
  expected: number,
): Promise<WorkspaceMigrationStatus> {
  if (!journal || journal.pendingIds.length) return status();
  const control = await readControl(service, assertCurrent);
  if (
    !control.control ||
    control.control.migrationId !== journal.control.migrationId ||
    control.control.ownerId !== journal.control.ownerId ||
    control.control.manifestHash !== journal.control.manifestHash
  ) {
    throw new WorkspaceMigrationError("other-owner");
  }
  if (control.control.phase !== "complete") {
    const uploaded = await readCollection(
      service,
      V2_COLLECTION,
      assertCurrent,
    );
    const actual = new Map(
      uploaded.records.map((
        record,
      ) => [record.id, canonicalWorkspaceMigrationRecord(record)]),
    );
    for (const record of journal.snapshot.records) {
      if (actual.get(record.id) !== canonicalWorkspaceMigrationRecord(record)) {
        throw new WorkspaceMigrationError("write-not-acknowledged");
      }
    }
    await writeControl(
      service,
      { ...journal.control, phase: "complete" },
      control.modified,
      assertCurrent,
    );
  }
  journal.control.phase = "complete";
  activePhase = "complete";
  await persist(expected);
  return status();
}

/** One-way migration; this module never writes, resets or deletes the v1 collection. */
export const WorkspaceContainerMigration = {
  prepare(
    service: WorkspaceMigrationService,
    options: WorkspaceMigrationOptions,
  ): Promise<WorkspaceMigrationStatus> {
    const expected = epoch;
    const assertCurrent = () => {
      options.assertCurrent();
      if (expected !== epoch) {
        throw new WorkspaceMigrationError("account-changed");
      }
    };
    return enqueue(async () => {
      activePhase = null;
      assertCurrent();
      if (!options.accountUid || !isWorkspaceSyncId(options.deviceId)) {
        throw new WorkspaceMigrationError("invalid-record");
      }
      await load(options.accountUid, expected);
      assertCurrent();
      const remote = await readControl(service, assertCurrent);
      if (remote.control?.phase === "complete") {
        await ensureMetadata(service, false, assertCurrent);
        if (journal?.control.migrationId === remote.control.migrationId) {
          journal.control.phase = "complete";
          journal.pendingIds = [];
          await persist(expected);
        }
        activePhase = "complete";
        return { phase: "complete", migrationId: remote.control.migrationId };
      }
      if (remote.control) {
        if (remote.control.ownerId !== options.deviceId) {
          throw new WorkspaceMigrationError("other-owner");
        }
        if (
          !journal ||
          journal.control.migrationId !== remote.control.migrationId ||
          journal.control.manifestHash !== remote.control.manifestHash
        ) {
          throw new WorkspaceMigrationError("journal-missing");
        }
        await ensureMetadata(service, false, assertCurrent);
        activePhase = "pending";
        return finish(service, assertCurrent, expected);
      }
      const destination = await readCollection(
        service,
        V2_COLLECTION,
        assertCurrent,
      );
      if (destination.records.length) {
        throw new WorkspaceMigrationError("inconsistent-server");
      }
      if (journal?.control.phase === "complete") {
        throw new WorkspaceMigrationError("inconsistent-server");
      }
      if (journal && journal.deviceId !== options.deviceId) {
        throw new WorkspaceMigrationError("other-owner");
      }
      if (!journal) {
        const legacy = await readCollection(
          service,
          LEGACY_COLLECTION,
          assertCurrent,
        );
        const snapshot = buildWorkspaceMigrationSnapshot(
          legacy.records,
          options.localDocuments,
          options.localRetired,
          options.containerDefinitions,
          options.localMetadataKnown,
        );
        journal = {
          version: 1,
          accountUid: options.accountUid,
          deviceId: options.deviceId,
          control: {
            version: 1,
            kind: "workspace-container-migration",
            migrationId: Services.uuid.generateUUID().toString().replace(
              /[{}]/g,
              "",
            ),
            ownerId: options.deviceId,
            phase: "pending",
            sourceModified: legacy.modified,
            manifestHash: await manifestHash(snapshot.records),
            recordCount: snapshot.records.length,
          },
          snapshot,
          pendingIds: snapshot.records.map((record) => record.id),
        };
        assertCurrent();
      }
      // A prior failed durable write may have left only an in-memory snapshot.
      // Every claim attempt establishes the disk boundary again before CAS.
      await persist(expected);
      await ensureMetadata(service, true, assertCurrent);
      await writeControl(
        service,
        journal.control,
        remote.modified,
        assertCurrent,
      );
      activePhase = "pending";
      return finish(service, assertCurrent, expected);
    });
  },
  pendingRecords(): WorkspaceMigrationRecord[] {
    if (
      !journal || activePhase !== "pending" ||
      journal.control.phase === "complete"
    ) return [];
    const pending = new Set(journal.pendingIds);
    return structuredClone(
      journal.snapshot.records.filter((record) => pending.has(record.id)),
    );
  },
  acknowledge(
    service: WorkspaceMigrationService,
    sent: readonly WorkspaceMigrationRecord[],
    assertCurrent: () => void,
  ): Promise<WorkspaceMigrationStatus> {
    const expected = epoch;
    return enqueue(async () => {
      assertCurrent();
      if (expected !== epoch) {
        throw new WorkspaceMigrationError("account-changed");
      }
      if (!journal) throw new WorkspaceMigrationError("journal-missing");
      if (activePhase !== "pending") {
        throw new WorkspaceMigrationError("other-owner");
      }
      if (journal.control.phase === "complete") return status();
      const previous = journal.pendingIds;
      journal.pendingIds = acknowledgeWorkspaceMigrationRecords(
        journal.pendingIds,
        journal.snapshot.records,
        sent,
      );
      try {
        await persist(expected);
      } catch (error) {
        if (journal && expected === epoch) journal.pendingIds = previous;
        throw error;
      }
      return finish(service, assertCurrent, expected);
    });
  },
  resetAccount(): Promise<void> {
    ++epoch;
    journal = null;
    activeAccount = null;
    activePhase = null;
    const expected = epoch;
    return enqueue(() => persist(expected));
  },
};
