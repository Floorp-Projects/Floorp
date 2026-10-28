// SPDX-License-Identifier: MPL-2.0

import type {
  WorkspaceContainerDefinition,
  WorkspaceContainerRef,
} from "./workspace-container-types.ts";
import {
  mergeWorkspaceContainerDefinitions,
  parseWorkspaceContainerDefinition,
  parseWorkspaceContainerRef,
} from "./workspace-container.ts";

export const WORKSPACE_CONTAINER_BINDINGS_CHANGED =
  "floorp.workspaces.containers.changed";
export const WORKSPACE_CONTAINER_ASSOCIATION_CHANGED =
  "floorp.workspaces.containers.association-changed";

export interface LocalWorkspaceContainer {
  userContextId: number;
  name: string;
  color: string;
  icon: string;
}

export type WorkspaceContainerPendingReason =
  | "definition-missing"
  | "legacy-unresolved"
  | "confirmation"
  | "deleted"
  | "creating"
  | "disabled"
  | "deleting"
  | "creation-failed";

export type WorkspaceContainerResolution =
  | { status: "resolved"; userContextId: number; instanceId?: string }
  | {
    status: "pending";
    reason: WorkspaceContainerPendingReason;
    containerId?: string;
    definition?: WorkspaceContainerDefinition;
    candidates: LocalWorkspaceContainer[];
  };

interface LocalInstance {
  userContextId: number;
  instanceId: string;
  deleted: boolean;
}

interface ContainerBinding {
  status: "ready" | "confirmation" | "creating" | "deleted" | "creation-failed";
  userContextId?: number;
  instanceId?: string;
}

interface CreationJournal {
  operationId: string;
  containerId: string;
  existingIds: number[];
}

interface RemovalJournal {
  userContextIds: number[];
  reason: "delete" | "disable";
}

export interface WorkspaceContainerBindingState {
  version: 1;
  accountUid: string | null;
  deviceId: string;
  instances: Record<string, LocalInstance>;
  definitions: Record<string, WorkspaceContainerDefinition>;
  bindings: Record<string, ContainerBinding>;
  creations: Record<string, CreationJournal>;
  removals: Record<string, RemovalJournal>;
}

export interface WorkspaceContainerBindingDependencies {
  load(): Promise<unknown>;
  save(state: WorkspaceContainerBindingState): Promise<void>;
  list(): LocalWorkspaceContainer[];
  create(definition: WorkspaceContainerDefinition): LocalWorkspaceContainer;
  persistIdentities(): Promise<void>;
  supports(definition: WorkspaceContainerDefinition): boolean;
  enabled(): boolean;
  uuid(): string;
  now(): number;
  changed(): void;
  associationChanged?(containerId: string): void;
}

function object(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function uuid(value: unknown): value is string {
  return typeof value === "string" &&
    /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(
      value,
    );
}

function localId(value: unknown): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) && value > 0;
}

function blankState(deviceId: string): WorkspaceContainerBindingState {
  return {
    version: 1,
    accountUid: null,
    deviceId,
    instances: {},
    definitions: {},
    bindings: {},
    creations: {},
    removals: {},
  };
}

/** Invalid persisted state fails closed; it is never replaced with empty bindings. */
export function parseWorkspaceContainerBindingState(
  value: unknown,
): WorkspaceContainerBindingState {
  if (
    !object(value) || value.version !== 1 || !uuid(value.deviceId) ||
    !(value.accountUid === null || typeof value.accountUid === "string") ||
    !object(value.instances) || !object(value.definitions) ||
    !object(value.bindings) || !object(value.creations) ||
    !object(value.removals)
  ) throw new Error("Invalid workspace container binding storage");
  const result = blankState(value.deviceId);
  result.accountUid = value.accountUid;
  for (const [id, raw] of Object.entries(value.instances)) {
    if (
      !object(raw) || !localId(raw.userContextId) ||
      String(raw.userContextId) !== id || !uuid(raw.instanceId) ||
      typeof raw.deleted !== "boolean"
    ) throw new Error("Invalid workspace container instance");
    result.instances[id] = {
      userContextId: raw.userContextId,
      instanceId: raw.instanceId,
      deleted: raw.deleted,
    };
  }
  for (const [id, raw] of Object.entries(value.definitions)) {
    const definition = parseWorkspaceContainerDefinition(raw);
    if (definition.containerId !== id) {
      throw new Error("Workspace container definition ID mismatch");
    }
    result.definitions[id] = definition;
  }
  const statuses = new Set([
    "ready",
    "confirmation",
    "creating",
    "deleted",
    "creation-failed",
  ]);
  for (const [id, raw] of Object.entries(value.bindings)) {
    if (!uuid(id) || !object(raw) || !statuses.has(String(raw.status))) {
      throw new Error("Invalid workspace container binding");
    }
    if (
      (raw.userContextId !== undefined && !localId(raw.userContextId)) ||
      (raw.instanceId !== undefined && !uuid(raw.instanceId)) ||
      (raw.status === "ready" &&
        (!localId(raw.userContextId) || !uuid(raw.instanceId)))
    ) throw new Error("Invalid workspace container binding target");
    result.bindings[id] = {
      status: raw.status as ContainerBinding["status"],
      ...(localId(raw.userContextId)
        ? { userContextId: raw.userContextId }
        : {}),
      ...(uuid(raw.instanceId) ? { instanceId: raw.instanceId } : {}),
    };
  }
  for (const [id, raw] of Object.entries(value.creations)) {
    if (
      !uuid(id) || !object(raw) || raw.containerId !== id ||
      !uuid(raw.operationId) || !Array.isArray(raw.existingIds) ||
      !raw.existingIds.every(localId)
    ) throw new Error("Invalid workspace container creation journal");
    result.creations[id] = {
      operationId: raw.operationId,
      containerId: id,
      existingIds: [...raw.existingIds],
    };
  }
  for (const [id, raw] of Object.entries(value.removals)) {
    if (
      !uuid(id) || !object(raw) ||
      !(raw.reason === "delete" || raw.reason === "disable") ||
      !Array.isArray(raw.userContextIds) || !raw.userContextIds.every(localId)
    ) throw new Error("Invalid workspace container removal journal");
    result.removals[id] = {
      userContextIds: [...raw.userContextIds],
      reason: raw.reason,
    };
  }
  return result;
}

/** Profile-scoped coordinator, also usable with isolated identities/storage in tests. */
export class WorkspaceContainerBindingCoordinator {
  private state: WorkspaceContainerBindingState | undefined;
  private loaded: Promise<void> | undefined;
  private tail: Promise<void> = Promise.resolve();
  private epoch = 0;
  private requestedAccount: string | null | undefined;
  private revision = 0;
  private savedRevision = 0;
  private persistenceFailure: unknown;
  private promptActive = false;
  private accountInvalidated = false;

  constructor(
    private readonly dependencies: WorkspaceContainerBindingDependencies,
  ) {}

  private data(): WorkspaceContainerBindingState {
    if (!this.state) {
      throw new Error("Workspace container bindings are not ready");
    }
    return this.state;
  }

  private assertEpoch(expected: number): void {
    if (expected !== this.epoch) {
      throw new Error("Workspace container account changed");
    }
  }

  private queue<T>(operation: () => T | Promise<T>): Promise<T> {
    const result = this.tail.then(operation);
    this.tail = result.then(() => {}, () => {});
    return result;
  }

  private dirty(): void {
    this.revision++;
    this.dependencies.changed();
  }

  private async persist(expected: number): Promise<void> {
    this.assertEpoch(expected);
    while (this.savedRevision < this.revision) {
      const revision = this.revision;
      const snapshot = structuredClone(this.data());
      try {
        await this.dependencies.save(snapshot);
      } catch (error) {
        this.persistenceFailure = error;
        throw error;
      }
      this.assertEpoch(expected);
      this.savedRevision = revision;
      this.persistenceFailure = undefined;
    }
  }

  private load(): Promise<void> {
    return this.loaded ??= this.dependencies.load().then((raw) => {
      this.state = raw === null || raw === undefined
        ? blankState(this.dependencies.uuid())
        : parseWorkspaceContainerBindingState(raw);
      // A native create may have completed before a crash, without our binding.
      // Never repeat it automatically or claim a same-named candidate as ours.
      for (const id of Object.keys(this.state.creations)) {
        this.state.bindings[id] = { status: "confirmation" };
      }
      // A crash can occur after native identities were reset, but before the
      // completion notification. Reused default IDs are not proof of survival.
      // Require explicit reassociation even if a cancelled operation left the
      // original identity intact; never leave an unrecoverable deleting state.
      for (const operation of Object.values(this.state.removals)) {
        this.markDeleted(operation.userContextIds);
      }
      this.state.removals = {};
      this.reconcileInstances();
      this.dirty();
    });
  }

  ready(accountUid?: string | null, deviceId?: string): Promise<void> {
    if (this.accountInvalidated) {
      return Promise.reject(
        new Error("Workspace container account was invalidated"),
      );
    }
    if (
      accountUid !== undefined && accountUid !== this.requestedAccount
    ) {
      this.requestedAccount = accountUid;
      this.epoch++;
      this.promptActive = false;
    }
    const expected = this.epoch;
    return this.queue(async () => {
      await this.load();
      this.assertEpoch(expected);
      if (accountUid !== undefined && this.data().accountUid !== accountUid) {
        const instances = this.data().instances;
        const removals = this.data().removals;
        const anonymous = this.data().accountUid === null
          ? {
            definitions: this.data().definitions,
            bindings: this.data().bindings,
            creations: this.data().creations,
          }
          : undefined;
        this.state = blankState(deviceId ?? this.data().deviceId);
        this.state.accountUid = accountUid;
        this.state.instances = instances;
        // Native removals affect the profile even if account ownership changes.
        this.state.removals = removals;
        if (anonymous && accountUid !== null) {
          Object.assign(this.state, anonymous);
        }
        this.dirty();
      }
      if (deviceId !== undefined) {
        if (!uuid(deviceId)) {
          throw new Error("Invalid workspace container device ID");
        }
        if (this.data().deviceId !== deviceId) {
          this.data().deviceId = deviceId;
          this.dirty();
        }
      }
      this.reconcileInstances();
      await this.persist(expected);
    });
  }

  invalidateAccount(): void {
    // The native sign-out notification must fence even a not-yet-loaded service
    // synchronously. Only its owner's durable reset may make it ready again.
    // Observer order is unspecified: another listener may have queued that
    // reset already, so repeated invalidation must not cancel the same reset.
    if (this.accountInvalidated) return;
    this.epoch++;
    this.accountInvalidated = true;
    this.promptActive = false;
  }

  resetAccount(): Promise<void> {
    this.invalidateAccount();
    const expected = ++this.epoch;
    this.requestedAccount = null;
    return this.queue(async () => {
      await this.load();
      this.assertEpoch(expected);
      const previous = this.data();
      this.state = blankState(previous.deviceId);
      this.state.instances = previous.instances;
      this.state.removals = previous.removals;
      this.dirty();
      await this.persist(expected);
      this.accountInvalidated = false;
    });
  }

  isReady(): boolean {
    return Boolean(
      !this.accountInvalidated && this.state &&
        (this.requestedAccount === undefined ||
          this.state.accountUid === this.requestedAccount),
    );
  }

  getEpoch(): number {
    return this.epoch;
  }

  isCurrentEpoch(expected: number): boolean {
    return expected === this.epoch && this.isReady();
  }

  private assertReady(): void {
    if (!this.isReady()) {
      throw new Error("Workspace container account is not ready");
    }
  }

  listLocalContainers(): LocalWorkspaceContainer[] {
    return this.dependencies.list().map((identity) => ({ ...identity }));
  }

  acquirePrompt(): boolean {
    if (!this.isReady() || this.promptActive) return false;
    this.promptActive = true;
    return true;
  }

  releasePrompt(expectedEpoch = this.epoch): void {
    if (expectedEpoch === this.epoch) this.promptActive = false;
  }

  flush(): Promise<void> {
    const expected = this.epoch;
    return this.queue(async () => {
      await this.load();
      await this.persist(expected);
    });
  }

  private instance(identity: LocalWorkspaceContainer): LocalInstance {
    let value = this.data().instances[String(identity.userContextId)];
    if (!value || value.deleted) {
      value = {
        userContextId: identity.userContextId,
        instanceId: this.dependencies.uuid(),
        deleted: false,
      };
      this.data().instances[String(identity.userContextId)] = value;
      this.dirty();
    }
    return value;
  }

  private reconcileInstances(): void {
    const identities = this.dependencies.list();
    const ids = new Set(identities.map((identity) => identity.userContextId));
    for (const identity of identities) this.instance(identity);
    for (const instance of Object.values(this.data().instances)) {
      if (!instance.deleted && !ids.has(instance.userContextId)) {
        this.markDeleted([instance.userContextId]);
      }
    }
    for (const [id, binding] of Object.entries(this.data().bindings)) {
      if (binding.status !== "ready") continue;
      const instance = this.data().instances[String(binding.userContextId)];
      if (
        !instance || instance.deleted ||
        instance.instanceId !== binding.instanceId
      ) {
        this.data().bindings[id] = { ...binding, status: "deleted" };
        this.dirty();
      }
    }
  }

  noteCreated(userContextId: number): void {
    if (!this.state || !localId(userContextId)) return;
    // A created event starts a new lifetime even when a numeric ID is reused.
    this.markDeleted([userContextId]);
    this.data().instances[String(userContextId)] = {
      userContextId,
      instanceId: this.dependencies.uuid(),
      deleted: false,
    };
    this.dirty();
  }

  markDeleted(userContextIds: readonly number[]): void {
    if (!this.state) return;
    const ids = new Set(userContextIds);
    for (const instance of Object.values(this.data().instances)) {
      if (ids.has(instance.userContextId) && !instance.deleted) {
        instance.deleted = true;
        this.dirty();
      }
    }
    for (const binding of Object.values(this.data().bindings)) {
      if (
        binding.userContextId !== undefined && ids.has(binding.userContextId) &&
        binding.status !== "deleted"
      ) {
        binding.status = "deleted";
        this.dirty();
      }
    }
  }

  listDefinitions(): WorkspaceContainerDefinition[] {
    return Object.values(this.data().definitions).map((value) => ({
      ...value,
    }));
  }

  getDefinition(id: string): WorkspaceContainerDefinition | undefined {
    const value = this.data().definitions[id];
    return value ? { ...value } : undefined;
  }

  receiveDefinitions(
    definitions: readonly WorkspaceContainerDefinition[],
  ): Promise<void> {
    const parsed = definitions.map(parseWorkspaceContainerDefinition);
    const expected = this.epoch;
    return this.queue(async () => {
      await this.load();
      this.assertEpoch(expected);
      this.assertReady();
      for (const definition of parsed) {
        const previous = this.data().definitions[definition.containerId];
        const next = previous
          ? mergeWorkspaceContainerDefinitions(previous, definition)
          : definition;
        if (JSON.stringify(previous) !== JSON.stringify(next)) {
          this.data().definitions[definition.containerId] = next;
          this.dirty();
        }
      }
      await this.persist(expected);
    });
  }

  captureReference(
    userContextId: number,
    preferredRef?: WorkspaceContainerRef,
  ): { ref: WorkspaceContainerRef; definition?: WorkspaceContainerDefinition } {
    this.assertReady();
    if (userContextId === 0) return { ref: { kind: "none" } };
    if (!localId(userContextId)) throw new Error("Invalid local container ID");
    const identity = this.dependencies.list().find((item) =>
      item.userContextId === userContextId
    );
    if (!identity) throw new Error("Local container is unavailable");
    const instance = this.instance(identity);
    const matches = (id: string) => {
      const binding = this.data().bindings[id];
      return binding?.status === "ready" &&
        binding.userContextId === userContextId &&
        binding.instanceId === instance.instanceId &&
        Boolean(this.data().definitions[id]);
    };
    const preferred = preferredRef?.kind === "container"
      ? preferredRef.containerId
      : undefined;
    let id = preferred && matches(preferred)
      ? preferred
      : Object.keys(this.data().bindings).sort().find(matches);
    if (!id) {
      id = this.dependencies.uuid();
      const definition = parseWorkspaceContainerDefinition({
        version: 2,
        kind: "container",
        containerId: id,
        name: identity.name,
        color: identity.color,
        icon: identity.icon,
        modifiedAt: Math.max(0, this.dependencies.now()),
        modifiedBy: this.data().deviceId,
      });
      this.data().definitions[id] = definition;
      this.data().bindings[id] = {
        status: "ready",
        userContextId,
        instanceId: instance.instanceId,
      };
      this.dirty();
    }
    return {
      ref: { kind: "container", containerId: id },
      definition: this.getDefinition(id),
    };
  }

  private candidates(
    definition: WorkspaceContainerDefinition | undefined,
  ): LocalWorkspaceContainer[] {
    return definition
      ? this.dependencies.list().filter((identity) =>
        identity.name === definition.name
      )
        .map((identity) => ({ ...identity }))
      : [];
  }

  private pending(
    reason: WorkspaceContainerPendingReason,
    containerId?: string,
  ): WorkspaceContainerResolution {
    const definition = containerId
      ? this.getDefinition(containerId)
      : undefined;
    return {
      status: "pending",
      reason,
      ...(containerId ? { containerId } : {}),
      ...(definition ? { definition } : {}),
      candidates: this.candidates(definition),
    };
  }

  lookup(ref: WorkspaceContainerRef): WorkspaceContainerResolution {
    const parsed = parseWorkspaceContainerRef(ref);
    this.assertReady();
    if (parsed.kind === "none") return { status: "resolved", userContextId: 0 };
    if (parsed.kind === "legacy-unresolved") {
      return this.pending("legacy-unresolved");
    }
    const id = parsed.containerId;
    const binding = this.data().bindings[id];
    if (!this.dependencies.enabled()) return this.pending("disabled", id);
    if (!this.data().definitions[id]) {
      return this.pending("definition-missing", id);
    }
    if (!binding) return this.pending("confirmation", id);
    if (binding.status !== "ready") return this.pending(binding.status, id);
    const instance = this.data().instances[String(binding.userContextId)];
    const exists = this.dependencies.list().some((identity) =>
      identity.userContextId === binding.userContextId
    );
    if (
      !instance || instance.deleted || !exists ||
      instance.instanceId !== binding.instanceId
    ) {
      this.markDeleted(
        binding.userContextId === undefined ? [] : [binding.userContextId],
      );
      return this.pending("deleted", id);
    }
    const removal = Object.values(this.data().removals).find((item) =>
      item.userContextIds.includes(instance.userContextId)
    );
    if (removal) {
      return this.pending(
        removal.reason === "disable" ? "disabled" : "deleting",
        id,
      );
    }
    if (this.persistenceFailure !== undefined) {
      return this.pending("creation-failed", id);
    }
    return {
      status: "resolved",
      userContextId: instance.userContextId,
      instanceId: instance.instanceId,
    };
  }

  getState(containerId: string): WorkspaceContainerResolution {
    return this.lookup({ kind: "container", containerId });
  }

  listPending(): WorkspaceContainerResolution[] {
    return Object.keys(this.data().definitions).map((id) => this.getState(id))
      .filter((resolution) => resolution.status === "pending");
  }

  resolve(ref: WorkspaceContainerRef): Promise<WorkspaceContainerResolution> {
    const parsed = parseWorkspaceContainerRef(ref);
    const expected = this.epoch;
    return this.queue(async () => {
      await this.load();
      this.assertEpoch(expected);
      this.assertReady();
      if (parsed.kind !== "container") return this.lookup(parsed);
      const id = parsed.containerId;
      const definition = this.data().definitions[id];
      if (
        !this.dependencies.enabled() || !definition || this.data().bindings[id]
      ) {
        const result = this.lookup(parsed);
        await this.persist(expected);
        return result;
      }
      if (this.candidates(definition).length) {
        this.data().bindings[id] = { status: "confirmation" };
        this.dirty();
        await this.persist(expected);
        return this.lookup(parsed);
      }
      return await this.createNow(id, expected);
    });
  }

  bind(
    containerId: string,
    userContextId: number,
    expectedEpoch?: number,
  ): Promise<WorkspaceContainerResolution> {
    const expected = expectedEpoch ?? this.epoch;
    return this.queue(async () => {
      await this.load();
      this.assertEpoch(expected);
      this.assertReady();
      if (!this.data().definitions[containerId]) {
        throw new Error("Unknown synced container");
      }
      const identity = this.dependencies.list().find((item) =>
        item.userContextId === userContextId
      );
      if (!identity || !this.dependencies.enabled()) {
        throw new Error("Local container is unavailable");
      }
      if (
        Object.values(this.data().removals).some((item) =>
          item.userContextIds.includes(userContextId)
        )
      ) {
        throw new Error("Local container operation is in progress");
      }
      const instance = this.instance(identity);
      this.data().bindings[containerId] = {
        status: "ready",
        userContextId,
        instanceId: instance.instanceId,
      };
      // Signal before general capture notifications, including on a failed
      // durable save: the old physical context must not remint this reference.
      this.dependencies.associationChanged?.(containerId);
      delete this.data().creations[containerId];
      this.dirty();
      await this.persist(expected);
      return this.getState(containerId);
    });
  }

  create(
    containerId: string,
    expectedEpoch?: number,
  ): Promise<WorkspaceContainerResolution> {
    const expected = expectedEpoch ?? this.epoch;
    // A second queued click must not retry an uncertain native creation. A
    // later deliberate retry can acknowledge the journal already visible then.
    const acknowledgedCreation = this.state?.creations[containerId]
      ?.operationId;
    return this.queue(async () => {
      await this.load();
      this.assertEpoch(expected);
      // Repeated explicit clicks must not create another already-resolved target.
      const existing = this.getState(containerId);
      if (existing.status === "resolved") return existing;
      if (existing.reason === "disabled" || existing.reason === "deleting") {
        return existing;
      }
      const creation = this.data().creations[containerId];
      if (creation && creation.operationId !== acknowledgedCreation) {
        return this.pending("confirmation", containerId);
      }
      return await this.createNow(containerId, expected);
    });
  }

  private async createNow(
    id: string,
    expected: number,
  ): Promise<WorkspaceContainerResolution> {
    const definition = this.data().definitions[id];
    if (!definition) return this.pending("definition-missing", id);
    const disabled = () =>
      !this.dependencies.enabled() ||
      Object.values(this.data().removals).some((item) =>
        item.reason === "disable"
      );
    if (disabled()) return this.pending("disabled", id);
    if (!this.dependencies.supports(definition)) {
      this.data().bindings[id] = { status: "creation-failed" };
      this.dirty();
      await this.persist(expected);
      return this.pending("creation-failed", id);
    }
    const previousBinding = this.data().bindings[id];
    const journal: CreationJournal = {
      operationId: this.dependencies.uuid(),
      containerId: id,
      existingIds: this.dependencies.list().map((identity) =>
        identity.userContextId
      ),
    };
    this.data().creations[id] = journal;
    this.data().bindings[id] = { status: "creating" };
    this.dirty();
    try {
      await this.persist(expected); // Must finish before native creation.
    } catch (error) {
      this.assertEpoch(expected);
      this.data().bindings[id] = { status: "creation-failed" };
      this.dirty();
      throw error;
    }
    this.assertEpoch(expected);
    // Preference changes and native removal preflight can run while writing.
    // Do not create an identity outside the disable operation's ID snapshot.
    if (disabled()) {
      if (previousBinding) this.data().bindings[id] = previousBinding;
      else delete this.data().bindings[id];
      delete this.data().creations[id];
      this.dirty();
      await this.persist(expected);
      return this.pending("disabled", id);
    }
    try {
      const identity = this.dependencies.create(definition);
      const instance = this.instance(identity);
      await this.dependencies.persistIdentities();
      this.assertEpoch(expected);
      this.data().bindings[id] = {
        status: "ready",
        userContextId: identity.userContextId,
        instanceId: instance.instanceId,
      };
      this.dependencies.associationChanged?.(id);
      delete this.data().creations[id];
      this.dirty();
      await this.persist(expected);
      return this.lookup({ kind: "container", containerId: id });
    } catch (error) {
      this.assertEpoch(expected);
      this.data().bindings[id] = { status: "creation-failed" };
      // Keep the journal: native create might have succeeded before save failed.
      this.data().creations[id] = journal;
      this.dirty();
      await this.persist(expected);
      if (this.persistenceFailure !== undefined) throw error;
      return this.pending("creation-failed", id);
    }
  }

  hasBinding(userContextId: number): boolean {
    if (!this.state) return false;
    return Object.values(this.data().bindings).some((binding) =>
      binding.status === "ready" &&
      (userContextId === 0 || binding.userContextId === userContextId)
    );
  }

  containerIdsForLocalIds(userContextIds: readonly number[]): string[] {
    if (!this.state) return [];
    const ids = new Set(userContextIds);
    return Object.entries(this.data().bindings).filter(([, binding]) => {
      if (
        binding.status !== "ready" || binding.userContextId === undefined ||
        !ids.has(binding.userContextId)
      ) return false;
      const instance = this.data().instances[String(binding.userContextId)];
      return Boolean(
        instance && !instance.deleted &&
          instance.instanceId === binding.instanceId,
      );
    }).map(([id]) => id);
  }

  prepareRemoval(
    operationId: string,
    userContextId: number,
    reason: "delete" | "disable",
    userContextIds?: readonly number[],
  ): Promise<void> {
    if (!uuid(operationId)) {
      return Promise.reject(new Error("Invalid container removal operation"));
    }
    const expected = this.epoch;
    const prepare = () => {
      this.assertEpoch(expected);
      const ids = userContextIds
        ? [...userContextIds]
        : userContextId === 0
        ? this.dependencies.list().map((identity) => identity.userContextId)
        : [userContextId];
      this.data().removals[operationId] = { userContextIds: ids, reason };
      this.dirty();
    };
    // Freeze synchronously when ready, before another provider can open a tab.
    if (this.state) prepare();
    return this.queue(async () => {
      await this.load();
      if (!this.data().removals[operationId]) prepare();
      await this.persist(expected);
    });
  }

  finishRemoval(
    operationId: string,
    outcome: "completed" | "cancelled",
  ): Promise<void> {
    const expected = this.epoch;
    return this.queue(async () => {
      await this.load();
      this.assertEpoch(expected);
      const operation = this.data().removals[operationId];
      if (!operation) return;
      if (outcome === "completed") {
        // Native disable also resets identities, so its old IDs cannot be reused.
        this.markDeleted(operation.userContextIds);
      }
      delete this.data().removals[operationId];
      this.dirty();
      await this.persist(expected);
    });
  }

  resumeDisabled(): Promise<void> {
    const expected = this.epoch;
    return this.queue(async () => {
      await this.load();
      this.assertEpoch(expected);
      if (!this.dependencies.enabled()) return;
      // Re-enabling can race an in-flight native disable (and be disabled
      // again). Only the native completion notification may release its freeze.
      // Interrupted startup operations were already recovered by load().
      this.dirty();
      await this.persist(expected);
    });
  }
}
