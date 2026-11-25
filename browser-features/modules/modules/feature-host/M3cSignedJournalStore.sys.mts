// SPDX-License-Identifier: MPL-2.0

import { M0RecordCommitFailure } from "./M0FeatureHostAdapters.ts";
import type {
  M0PreparedRecordWrite,
  M0RecordStoreAdapter,
} from "./M0FeatureHostAdapters.types.ts";
import {
  admitM3cPackage,
  highWaterAfterM3cCommitFailure,
  type M3cIdentityCatalog,
  M3cJournalError,
  mergeM3cHighWater,
  serializeM3cSignedJournal,
  validateM3cSignedJournal,
} from "./M3cSignedJournal.ts";
import type {
  M3cHighWater,
  M3cSignedJournal,
} from "./M3cSignedJournal.types.ts";

const NAMESPACE = "floorp-hotfix-signed-tests-v1";
const RECORD_NAME = "state.json";
const TEMP_NAME = "state.json.tmp";

function child(parent: nsIFile, name: string): nsIFile {
  const file = parent.clone();
  file.append(name);
  return file;
}

function isRegularFile(file: nsIFile): boolean {
  return file.exists() && !file.isSymlink() && file.isFile();
}

function assertDirectory(file: nsIFile): void {
  if (!file.exists() || file.isSymlink() || !file.isDirectory()) {
    throw new M3cJournalError("invalid-journal");
  }
}

function ensureDirectory(
  file: nsIFile,
  onCreated?: (directory: nsIFile) => void,
): void {
  if (file.exists()) {
    if (file.isSymlink() || !file.isDirectory()) {
      throw new M3cJournalError("invalid-journal");
    }
    return;
  }
  const createdDirectory = file.clone();
  file.create(Components.interfaces.nsIFile.DIRECTORY_TYPE!, 0o700);
  onCreated?.(createdDirectory);
  assertDirectory(file);
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function isNewAdmission(previous: unknown, next: unknown): boolean {
  if (!isRecord(previous) || previous.pending !== null || !isRecord(next)) {
    return false;
  }
  const pending = next.pending;
  return isRecord(pending) && pending.phase === "PREPARED" &&
    (pending.op === "install" || pending.op === "update");
}

export class M3cSignedJournalStore implements M0RecordStoreAdapter {
  readonly #featureId: string;
  readonly #testId: string;
  readonly #profile: nsIFile;
  readonly #namespace: nsIFile;
  readonly #testRoot: nsIFile;
  readonly #root: nsIFile;
  readonly #recordPath: string;
  readonly #tempPath: string;
  readonly #catalog: M3cIdentityCatalog;
  #highWater: M3cHighWater | null = null;
  #journal: M3cSignedJournal | null = null;
  readonly #createdDirectories: nsIFile[] = [];
  #partialCreateCleanupSafe = true;
  #initialJournalUsable = false;

  constructor(
    testId: string,
    featureId: string,
    catalog: M3cIdentityCatalog,
  ) {
    this.#testId = testId;
    this.#featureId = featureId;
    this.#catalog = catalog;
    this.#profile = Services.dirsvc.get(
      "ProfD",
      Components.interfaces.nsIFile,
    );
    this.#namespace = child(this.#profile, NAMESPACE);
    this.#testRoot = child(this.#namespace, testId);
    this.#root = child(this.#testRoot, featureId);
    this.#recordPath = child(this.#root, RECORD_NAME).path;
    this.#tempPath = child(this.#root, TEMP_NAME).path;
  }

  get rootPath(): string {
    return this.#root.path;
  }

  rootExists(): boolean {
    this.#assertProfile();
    if (!this.#namespace.exists()) return false;
    if (this.#namespace.isSymlink() || !this.#namespace.isDirectory()) {
      throw new M3cJournalError("invalid-journal");
    }
    if (!this.#testRoot.exists()) return false;
    if (this.#testRoot.isSymlink() || !this.#testRoot.isDirectory()) {
      throw new M3cJournalError("invalid-journal");
    }
    if (!this.#root.exists()) return false;
    if (this.#root.isSymlink() || !this.#root.isDirectory()) {
      throw new M3cJournalError("invalid-journal");
    }
    return true;
  }

  async createEmpty(
    lifecycle: unknown,
    failAfterRootCreationForTests = false,
  ): Promise<void> {
    this.#assertProfile();
    const onCreated = (directory: nsIFile) =>
      this.#createdDirectories.push(directory);
    ensureDirectory(this.#namespace, onCreated);
    ensureDirectory(this.#testRoot, onCreated);
    if (this.#root.exists()) throw new M3cJournalError("invalid-journal");
    ensureDirectory(this.#root, onCreated);
    if (failAfterRootCreationForTests) {
      throw new M3cJournalError("invalid-journal");
    }
    const journal = validateM3cSignedJournal({
      schemaVersion: 1,
      featureId: this.#featureId,
      highWater: null,
      packages: [],
      lifecycle,
    }, this.#featureId);
    const bytes = serializeM3cSignedJournal(journal);
    try {
      await this.#replace(bytes, journal, null, true);
    } catch (error) {
      if (error instanceof M0RecordCommitFailure && error.uncertain) {
        this.#partialCreateCleanupSafe = false;
      }
      throw error;
    }
    this.#initialJournalUsable = true;
  }

  async cleanupPartialCreate(): Promise<boolean> {
    if (!this.#partialCreateCleanupSafe || this.#initialJournalUsable) {
      return false;
    }
    for (const directory of [...this.#createdDirectories].reverse()) {
      if (!directory.exists()) continue;
      if (directory.isSymlink() || !directory.isDirectory()) return false;
      const entries = directory.directoryEntries;
      const hasEntries = entries.nextFile !== null;
      entries.close();
      if (hasEntries) {
        if (directory.path === this.#root.path) return false;
        continue;
      }
      try {
        await IOUtils.remove(directory.path);
      } catch {
        return false;
      }
    }
    return true;
  }

  async read(): Promise<unknown | null> {
    this.#assertLayout(true);
    let bytes: Uint8Array;
    try {
      bytes = await IOUtils.read(this.#recordPath, {
        maxBytes: 256 * 1024 + 1,
      });
    } catch {
      throw new M3cJournalError("invalid-journal");
    }
    if (bytes.length > 256 * 1024) {
      throw new M3cJournalError("journal-too-large");
    }
    let decoded: string;
    try {
      decoded = new TextDecoder("utf-8", { fatal: true }).decode(bytes);
    } catch {
      throw new M3cJournalError("invalid-journal");
    }
    let parsed: unknown;
    try {
      parsed = JSON.parse(decoded);
    } catch {
      throw new M3cJournalError("invalid-journal");
    }
    const journal = validateM3cSignedJournal(parsed, this.#featureId);
    for (const metadata of journal.packages) this.#catalog.add(metadata);
    this.#highWater = mergeM3cHighWater(this.#highWater, journal.highWater);
    this.#journal = journal;
    return journal.lifecycle;
  }

  prepare(next: unknown, previous: unknown): M0PreparedRecordWrite {
    if (!this.#journal) throw new M3cJournalError("invalid-journal");
    let candidateHighWater = this.#highWater;
    if (isNewAdmission(previous, next)) {
      if (!isRecord(next) || !isRecord(next.pending)) {
        throw new M3cJournalError("invalid-journal");
      }
      const token = next.pending.targetVersion;
      const metadata = typeof token === "string"
        ? this.#catalog.get(token)
        : null;
      if (!metadata) throw new M3cJournalError("invalid-journal");
      candidateHighWater = admitM3cPackage(this.#highWater, metadata);
    }
    const journal = validateM3cSignedJournal({
      schemaVersion: 1,
      featureId: this.#featureId,
      highWater: candidateHighWater,
      packages: this.#catalog.values(),
      lifecycle: next,
    }, this.#featureId);
    const bytes = serializeM3cSignedJournal(journal);
    return {
      commit: async () => {
        await this.#replace(bytes, journal, candidateHighWater, false);
      },
    };
  }

  async #replace(
    bytes: Uint8Array,
    journal: M3cSignedJournal,
    candidateHighWater: M3cHighWater | null,
    initial: boolean,
  ): Promise<void> {
    try {
      this.#assertLayout(!initial);
    } catch {
      throw new M0RecordCommitFailure(false);
    }
    const temp = child(this.#root, TEMP_NAME);
    const record = child(this.#root, RECORD_NAME);
    if (temp.exists()) {
      if (!isRegularFile(temp)) {
        throw new M0RecordCommitFailure(false);
      }
      try {
        await IOUtils.remove(temp.path);
      } catch {
        throw new M0RecordCommitFailure(false);
      }
    }
    try {
      await IOUtils.write(temp.path, bytes, { mode: "create", flush: true });
    } catch {
      throw new M0RecordCommitFailure(false);
    }
    try {
      await IOUtils.move(temp.path, record.path, { noOverwrite: false });
    } catch {
      this.#highWater = highWaterAfterM3cCommitFailure(
        this.#highWater,
        candidateHighWater,
        true,
      );
      throw new M0RecordCommitFailure(true);
    }
    this.#highWater = mergeM3cHighWater(
      this.#highWater,
      journal.highWater,
    );
    this.#journal = journal;
  }

  #assertProfile(): void {
    if (this.#profile.isSymlink() || !this.#profile.isDirectory()) {
      throw new M3cJournalError("invalid-journal");
    }
  }

  #assertLayout(requireRecord: boolean): void {
    this.#assertProfile();
    assertDirectory(this.#namespace);
    assertDirectory(this.#testRoot);
    assertDirectory(this.#root);
    const entries = this.#root.directoryEntries;
    let recordSeen = false;
    let tempSeen = false;
    try {
      let count = 0;
      for (
        let file = entries.nextFile;
        file !== null;
        file = entries.nextFile
      ) {
        count += 1;
        if (count > 2 || file.isSymlink() || !file.isFile()) {
          throw new M3cJournalError("invalid-journal");
        }
        if (file.leafName === RECORD_NAME) recordSeen = true;
        else if (file.leafName === TEMP_NAME) tempSeen = true;
        else throw new M3cJournalError("invalid-journal");
      }
    } finally {
      entries.close();
    }
    if (
      requireRecord &&
      (!recordSeen || !isRegularFile(child(this.#root, RECORD_NAME)))
    ) {
      throw new M3cJournalError("invalid-journal");
    }
    if (tempSeen && !isRegularFile(child(this.#root, TEMP_NAME))) {
      throw new M3cJournalError("invalid-journal");
    }
  }
}
