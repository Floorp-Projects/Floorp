// SPDX-License-Identifier: MPL-2.0
// Manual browser scenario; only the isolated-profile driver invokes this.
// deno-lint-ignore no-unused-vars
async function runScenario() {
  if (
    !Services.prefs.getBoolPref("floorp.workspaces.sync.manual-test", false)
  ) throw new Error("Dedicated test profile required");
  const win = Services.wm.getMostRecentWindow("navigator:browser");
  const module = ChromeUtils.importESModule(
    "resource://floorp-workspace-sync-validation/engine.sys.mjs",
  );
  const model = ChromeUtils.importESModule(
    "resource://floorp-workspace-sync-validation/model.sys.mjs",
  );
  const { WorkspaceContainerBindings: bindings } = ChromeUtils.importESModule(
    "resource://floorp-workspace-sync-validation/container-bindings.sys.mjs",
  );
  const { Service } = ChromeUtils.importESModule(
    "resource://services-sync/service.sys.mjs",
  );
  await Service.promiseInitialized;
  const {
    FloorpWorkspacesSync: api,
    FloorpWorkspacesEngine,
    FloorpWorkspaceRecord,
  } = module;
  const assert = (condition, message) => {
    if (!condition) throw new Error(message);
  };
  const uuid = () =>
    Services.uuid.generateUUID().toString().replace(/[{}]/g, "");
  const completed = [];
  const workspace = uuid(), tabA = uuid(), tabB = uuid(), device = uuid();
  const snapshots = new Map();
  const closed = [];
  const applyCalls = new Map();
  let nextApplyGate = null;
  let failNextApply = false;
  let beforeCaptureCheck = null;
  let beforeApplyCheck = null;
  const makeSnapshot = (id, tabs) => ({
    version: 2,
    workspaceId: id,
    name: "Research",
    icon: "book",
    defaultContainer: { kind: "none" },
    tabs: tabs.map((id) => ({
      id,
      url: "https://example.com/" + id,
      title: id,
      pinned: false,
      container: { kind: "none" },
    })),
  });
  snapshots.set(workspace, makeSnapshot(workspace, [tabA]));
  const provider = {
    window: win,
    capture: (id) => {
      beforeCaptureCheck?.(id);
      const snapshot = snapshots.get(id);
      return snapshot
        ? {
          ...snapshot,
          tabs: snapshot.tabs.filter((tab) =>
            !api.isPendingClose(tab.id) && !api.isRetired(tab.id)
          ),
        }
        : null;
    },
    tabIds: (id) => snapshots.get(id)?.tabs.map((tab) => tab.id) ?? [],
    containerTabIds: (localIds) => {
      const logicalIds = new Set(bindings.containerIdsForLocalIds(localIds));
      return Object.fromEntries([...snapshots].map(([id, snapshot]) => [
        id,
        snapshot.tabs.filter((tab) =>
          tab.container?.kind === "container" &&
          logicalIds.has(tab.container.containerId)
        ).map((tab) => tab.id),
      ]));
    },
    apply: async (document, _missingTabIds, signal) => {
      await beforeApplyCheck?.(document);
      applyCalls.set(
        document.workspaceId,
        (applyCalls.get(document.workspaceId) ?? 0) + 1,
      );
      if (nextApplyGate) {
        const gate = nextApplyGate;
        nextApplyGate = null;
        gate.entered();
        await gate.release;
      }
      if (signal?.aborted) {
        throw new Error("Workspace apply cancelled after account change");
      }
      if (failNextApply) {
        failNextApply = false;
        throw new Error("simulated provider UI failure");
      }
      closed.push(
        ...document.tabs.filter((tab) => tab.deleted).map((tab) => tab.id),
      );
      const pendingContainerTabIds = [];
      const materialized = [];
      for (const tab of model.liveWorkspaceSyncTabs(document)) {
        const resolution = await bindings.resolve(tab.container);
        if (resolution.status === "pending") {
          pendingContainerTabIds.push(tab.id);
        } else materialized.push(tab);
      }
      const defaultResolution = await bindings.resolve(
        document.defaultContainer,
      );
      if (signal?.aborted) {
        throw new Error("Workspace apply cancelled after account change");
      }
      snapshots.set(document.workspaceId, {
        version: 2,
        workspaceId: document.workspaceId,
        name: document.name,
        icon: document.icon,
        defaultContainer: document.defaultContainer,
        tabs: materialized.map((
          { id, url, title, pinned, container },
        ) => ({ id, url, title, pinned, container })),
      });
      return {
        pendingContainerTabIds,
        pendingDefaultContainer: defaultResolution.status === "pending",
      };
    },
  };
  const { JSONFile } = ChromeUtils.importESModule(
    "resource://gre/modules/JSONFile.sys.mjs",
  );
  let cache;
  let releaseInitialLoad, enteredInitialLoad;
  const initialLoadEntered = new Promise((resolve) =>
    enteredInitialLoad = resolve
  );
  const initialLoadRelease = new Promise((resolve) =>
    releaseInitialLoad = resolve
  );
  const originalLoad = JSONFile.prototype.load;
  JSONFile.prototype.load = function (...args) {
    if (
      PathUtils.filename(this.path) === "floorp-workspaces-sync-v2-manual.json"
    ) {
      cache = this;
      enteredInitialLoad();
      return initialLoadRelease.then(() => originalLoad.apply(this, args));
    }
    return originalLoad.apply(this, args);
  };
  let unregister;
  try {
    const staleInitialReady = api.ready().then(() => null, (error) => error);
    await initialLoadEntered;
    Services.obs.notifyObservers(null, "weave:service:start-over");
    const readinessAfterReset = api.ready();
    releaseInitialLoad();
    assert(
      await staleInitialReady,
      "startOver rejects the pending initial account load",
    );
    await readinessAfterReset;
    assert(
      !Object.keys(cache.data.records).length &&
        !bindings.listDefinitions().length,
      "readiness after reset starts empty without retaining a rejected initialization promise",
    );
    unregister = await api.register(provider);
  } finally {
    JSONFile.prototype.load = originalLoad;
  }
  assert(
    cache,
    "isolated engine state is available for persistence regression",
  );
  const engine = new FloorpWorkspacesEngine(Service);
  await engine.initialize();
  Services.prefs.setBoolPref("services.sync.engine.floorp-workspaces-v2", true);
  engine._tracker.start();
  const record = (id, entity) => {
    const result = new FloorpWorkspaceRecord("floorp-workspaces-v2", id);
    result.entity = entity;
    return result;
  };
  const tabEntity = (id, parent = workspace) => ({
    version: 2,
    kind: "tab",
    workspaceId: parent,
    tab: {
      id,
      url: "https://example.com/" + id,
      title: id,
      pinned: false,
      container: { kind: "none" },
      position: 1,
      deleted: false,
      modifiedAt: Date.now() + 1000,
      modifiedBy: device,
    },
  });
  const metaEntity = (id) => ({
    version: 2,
    kind: "workspace",
    workspaceId: id,
    name: "Remote",
    icon: null,
    defaultContainer: { kind: "none" },
    metadata: { modifiedAt: Date.now() + 1000, modifiedBy: device },
  });
  const tomb = (id) => {
    const result = new FloorpWorkspaceRecord("floorp-workspaces-v2", id);
    result.deleted = true;
    return result;
  };
  completed.push(
    "startOver during initial load fences stale readiness and allows clean initialization",
  );

  // A pre-existing account has metadata for Firefox engines, but no entry for
  // the newly installed Floorp engine. Exercise Gecko's actual reconciliation,
  // which interprets that missing entry as a remote disable even with our
  // shipped default=true. An explicit workspace opt-in must override it.
  const { EngineSynchronizer } = ChromeUtils.importESModule(
    "resource://services-sync/stages/enginesync.sys.mjs",
  );
  const synchronizer = new EngineSynchronizer(Service);
  const manager = {
    get enabledEngineNames() {
      return engine.enabled ? ["floorp-workspaces-v2"] : [];
    },
    get: (name) => name === "floorp-workspaces-v2" ? engine : null,
    decline: () => {},
    undecline: () => {},
  };
  const oldAccountMeta = {
    isNew: false,
    payload: { engines: { bookmarks: { version: 2 } }, declined: [] },
  };
  Services.prefs.clearUserPref(
    "services.sync.engineStatusChanged.floorp-workspaces-v2",
  );
  await synchronizer._updateEnabledFromMeta(oldAccountMeta, 2, manager);
  assert(
    !Services.prefs.getBoolPref("services.sync.engine.floorp-workspaces-v2"),
    "native reconciliation reproduces the existing-account remote disable",
  );
  await api.select(workspace, true);
  assert(
    Services.prefs.getBoolPref("services.sync.engine.floorp-workspaces-v2"),
    "explicit workspace selection enables the native Sync engine",
  );
  assert(
    Services.prefs.getBoolPref(
      "services.sync.engineStatusChanged.floorp-workspaces-v2",
      false,
    ),
    "workspace opt-in is recorded as a local engine enablement",
  );
  await new Promise((resolve) => win.setTimeout(resolve, 0));
  await synchronizer._updateEnabledFromMeta(oldAccountMeta, 2, manager);
  assert(
    Services.prefs.getBoolPref("services.sync.engine.floorp-workspaces-v2"),
    "old account metadata cannot silently disable an explicit workspace opt-in",
  );
  completed.push(
    "existing-account opt-in survives native engine reconciliation",
  );
  Services.prefs.setBoolPref(
    "services.sync.engine.floorp-workspaces-v2",
    false,
  );
  await api.capture();
  assert(
    !Services.prefs.getBoolPref("services.sync.engine.floorp-workspaces-v2"),
    "background capture respects disabled workspace transport",
  );
  await api.select(workspace, false);
  assert(
    !Services.prefs.getBoolPref("services.sync.engine.floorp-workspaces-v2"),
    "opting out does not enable workspace transport",
  );
  await api.select(workspace, true);
  completed.push("only explicit opt-in resumes disabled workspace transport");
  const ids = await engine._store.getAllIDs();
  assert(
    Object.keys(ids).length === 2 && ids[workspace] && ids[tabA],
    "selected workspace emits metadata and separate tab records",
  );
  const outgoing = await engine._store.createRecord(
    tabA,
    "floorp-workspaces-v2",
  );
  assert(
    outgoing.entity.kind === "tab" && !Object.hasOwn(outgoing.entity, "tabs"),
    "tab payload is independent",
  );
  assert(
    engine.name === "floorp-workspaces-v2" &&
      outgoing.entity.version === 2 &&
      outgoing.entity.tab.container.kind === "none",
    "native v2 collection emits explicit container references",
  );
  const { BulkKeyBundle } = ChromeUtils.importESModule(
    "resource://services-sync/keys.sys.mjs",
  );
  const key = new BulkKeyBundle("floorp-workspaces-v2");
  await key.generateRandom();
  const before = JSON.stringify(outgoing.entity);
  await outgoing.encrypt(key);
  assert(
    outgoing.ciphertext && outgoing.cleartext === null,
    "native Sync encryption removes plaintext",
  );
  await outgoing.decrypt(key);
  assert(
    JSON.stringify(outgoing.entity) === before,
    "native encryption roundtrip preserves entity",
  );
  completed.push("selected per-record upload and native encryption");
  const incoming = record(tabB, tabEntity(tabB));
  assert(await engine._reconcile(incoming), "new tab reconciles");
  await engine._store.create(incoming);
  assert(
    snapshots.get(workspace).tabs.length === 2,
    "incoming tab merges with existing local tab",
  );
  completed.push("concurrent additions merge");
  const changeLocalURL = (url) => {
    const current = snapshots.get(workspace);
    snapshots.set(workspace, {
      ...current,
      tabs: current.tabs.map((tab) => tab.id === tabA ? { ...tab, url } : tab),
    });
  };
  const localURLSurvives = (url) =>
    snapshots.get(workspace).tabs.find((tab) => tab.id === tabA)?.url === url;
  const tabC = uuid();
  changeLocalURL("https://example.com/local-before-incoming-tab");
  await engine._store.create(record(tabC, tabEntity(tabC)));
  assert(
    localURLSurvives("https://example.com/local-before-incoming-tab"),
    "unrelated incoming tab preserves local navigation before debounced capture",
  );
  changeLocalURL("https://example.com/local-before-incoming-metadata");
  await engine._store.create(record(workspace, metaEntity(workspace)));
  assert(
    localURLSurvives("https://example.com/local-before-incoming-metadata"),
    "incoming metadata preserves local navigation before debounced capture",
  );
  completed.push("incoming tab and metadata preserve pending local navigation");
  await engine._reconcile(tomb(tabB));
  await engine._store.remove(tomb(tabB));
  assert(
    !closed.includes(tabB) && api.isPendingClose(tabB),
    "tab UI close waits across incoming batches",
  );
  changeLocalURL("https://example.com/local-before-close-flush");
  await engine._flushIncomingTabs();
  assert(
    closed.includes(tabB) &&
      !snapshots.get(workspace).tabs.some((tab) => tab.id === tabB),
    "completed incoming closes tab",
  );
  assert(
    localURLSurvives("https://example.com/local-before-close-flush"),
    "remote close flush preserves another tab's pending local navigation",
  );
  completed.push("incoming close flush preserves pending local navigation");
  assert(
    !await engine._reconcile(record(tabB, tabEntity(tabB))),
    "retired UUID never revives",
  );
  assert(
    (await engine._store.createRecord(tabB, "floorp-workspaces-v2")).deleted,
    "stale live records produce standard tombstone",
  );
  await engine._onRecordsWritten([]);
  assert(
    (await engine._store.getAllIDs())[tabB],
    "unacknowledged tombstone retained",
  );
  await engine._onRecordsWritten([uuid()]);
  assert(
    (await engine._store.getAllIDs())[tabB],
    "partial acknowledgement does not collect another tombstone",
  );
  await engine._onRecordsWritten([tabB]);
  assert(
    !(await engine._store.getAllIDs())[tabB] && api.isRetired(tabB),
    "successful tombstone upload is collected while UUID remains retired",
  );
  completed.push("delete wins and acknowledged tombstone collection");
  await api.select(workspace, false);
  assert(
    !api.isSelected(workspace) &&
      !await engine._reconcile(record(tabA, tabEntity(tabA))),
    "unsubscribe suppresses incoming without deleting local content",
  );
  assert(
    snapshots.get(workspace).tabs.some((tab) => tab.id === tabA),
    "unsubscribe preserves tabs",
  );
  completed.push("local unsubscribe preserves tabs");
  const deletedParent = uuid(), child = uuid();
  await engine._store.remove(tomb(deletedParent));
  assert(
    !await engine._reconcile(record(child, tabEntity(child, deletedParent))),
    "unknown parent tombstone suppresses later child",
  );
  assert(
    !await engine._reconcile(record(deletedParent, metaEntity(deletedParent))),
    "unknown parent tombstone suppresses later metadata",
  );
  completed.push("unknown parent tombstone order");
  const parent = uuid(), child2 = uuid();
  await engine._store.create(record(parent, metaEntity(parent)));
  await engine._store.create(record(child2, tabEntity(child2, parent)));
  await engine._store.remove(tomb(child2));
  const closesBefore = closed.length;
  await engine._store.remove(tomb(parent));
  await engine._flushIncomingTabs();
  assert(
    closed.length === closesBefore &&
      snapshots.get(parent).tabs.some((tab) => tab.id === child2),
    "parent tombstone in later batch preserves local workspace tabs",
  );
  completed.push("parent deletion across batches preserves local tabs");
  const failureParent = uuid(), failureChild = uuid();
  await engine._store.create(record(failureParent, metaEntity(failureParent)));
  await engine._store.create(
    record(failureChild, tabEntity(failureChild, failureParent)),
  );
  await engine._store.remove(tomb(failureChild));
  const broken = tomb(failureParent);
  broken.modified = Date.now() / 1000;
  broken.decrypt = () => Promise.reject(new Error("simulated corrupt parent"));
  await engine._maybeReconcile(broken);
  await engine._maybeReconcile(broken);
  const { SyncEngine } = ChromeUtils.importESModule(
    "resource://services-sync/engines.sys.mjs",
  );
  const originalProcess = SyncEngine.prototype._processIncoming;
  try {
    SyncEngine.prototype._processIncoming = function () {
      this.previousFailedIn.clear();
      this.toFetch.clear();
      return Promise.resolve();
    };
    await engine._processIncoming();
    await engine._processIncoming();
    assert(
      engine.toFetch.has(failureParent) && !closed.includes(failureChild),
      "failure remains backlogged after base retries are pruned across subsequent cycles",
    );
    const repaired = tomb(failureParent);
    repaired.modified = Date.now() / 1000;
    repaired.decrypt = () => Promise.resolve();
    assert(
      (await engine._maybeReconcile(repaired)).shouldApply,
      "repaired parent reconciles",
    );
    assert(
      (await engine._applyRecords([repaired], {
        addIncomingFailedReason: () => {},
      })).length === 0,
      "repaired parent applies",
    );
    await engine._processIncoming();
    assert(
      !engine.toFetch.has(failureParent) && !closed.includes(failureChild),
      "repair clears failure and parent deletion preserves tabs",
    );
  } finally {
    SyncEngine.prototype._processIncoming = originalProcess;
  }
  completed.push(
    "repeated incoming failure survives native retry pruning and recovers",
  );
  Services.prefs.setBoolPref("floorp.workspaces.enabled", false);
  assert(
    engine.enabled && engine.shouldSkipSync(),
    "disabled feature skips transport without changing the native engine preference",
  );
  const originalWipeServer = engine.wipeServer;
  let disabledWipes = 0;
  try {
    engine.wipeServer = () => {
      disabledWipes++;
      return Promise.resolve();
    };
    await synchronizer._updateEnabledFromMeta(
      {
        isNew: false,
        payload: {
          engines: {
            "floorp-workspaces-v2": { version: 1, syncID: engine.syncID },
          },
          declined: [],
        },
      },
      2,
      manager,
    );
    assert(
      disabledWipes === 0,
      "native metadata reconciliation never wipes the collection when the workspace feature is disabled",
    );
  } finally {
    engine.wipeServer = originalWipeServer;
  }
  await api.capture();
  Services.prefs.setBoolPref("floorp.workspaces.enabled", true);
  completed.push("disabled feature never blocks Sync");
  const disabledParent = uuid(), disabledChild = uuid();
  const disabledRecords = [
    record(disabledParent, metaEntity(disabledParent)),
    record(disabledChild, tabEntity(disabledChild, disabledParent)),
  ];
  const incomingTelemetry = { addIncomingFailedReason: () => {} };
  Services.prefs.setBoolPref("floorp.workspaces.enabled", false);
  const disabledFailures = await engine._applyRecords(
    disabledRecords,
    incomingTelemetry,
  );
  assert(
    disabledFailures.includes(disabledParent) &&
      disabledFailures.includes(disabledChild) &&
      !snapshots.has(disabledParent) && !api.isSelected(disabledParent),
    "an incoming download completing after disable changes neither selection nor UI",
  );
  assert(
    cache.data.incomingFailures.includes(disabledParent) &&
      cache.data.incomingFailures.includes(disabledChild),
    "disabled incoming IDs retain independent persisted retry markers",
  );
  let outgoingWhileDisabled = false;
  try {
    await engine._store.createRecord(tabA, "floorp-workspaces-v2");
    outgoingWhileDisabled = true;
  } catch { /* A previously started native upload may not read more tabs. */ }
  assert(!outgoingWhileDisabled, "disabled outgoing reads reject");
  Services.prefs.setStringPref(
    "services.sync.floorp-workspaces-v2.lastSync",
    "12345",
  );
  Services.prefs.setBoolPref("floorp.workspaces.enabled", true);
  await api.capture();
  assert(
    Services.prefs.getStringPref(
      "services.sync.floorp-workspaces-v2.lastSync",
    ) ===
      "0",
    "re-enabling refetches even if the native download advanced lastSync",
  );
  const disabledOriginalProcess = SyncEngine.prototype._processIncoming;
  try {
    SyncEngine.prototype._processIncoming = async () => {};
    engine.previousFailedIn.clear();
    engine.toFetch.clear();
    await engine._processIncoming();
    assert(
      engine.toFetch.has(disabledParent) && engine.toFetch.has(disabledChild),
      "disabled records remain fetchable after native repeated-failure pruning",
    );
  } finally {
    SyncEngine.prototype._processIncoming = disabledOriginalProcess;
  }
  assert(
    (await engine._applyRecords(disabledRecords, incomingTelemetry)).length ===
        0 &&
      api.isSelected(disabledParent) &&
      snapshots.get(disabledParent)?.tabs.some((tab) =>
        tab.id === disabledChild
      ),
    "replaying disabled records applies the complete workspace on re-enable",
  );
  completed.push("disabled in-flight records persist and replay on re-enable");

  Services.prefs.setBoolPref("floorp.workspaces.enabled", false);
  assert(
    (await engine._applyRecords([tomb(disabledChild)], incomingTelemetry))
      .includes(disabledChild) &&
      !api.isRetired(disabledChild) &&
      snapshots.get(disabledParent)?.tabs.some((tab) =>
        tab.id === disabledChild
      ),
    "incoming tab tombstones do not alter disabled local state",
  );
  Services.prefs.setBoolPref("floorp.workspaces.enabled", true);
  await api.capture();
  assert(
    (await engine._applyRecords([tomb(disabledChild)], incomingTelemetry))
      .length === 0,
    "the disabled tombstone is replayed after re-enable",
  );
  await engine._flushIncomingTabs();
  assert(
    !snapshots.get(disabledParent)?.tabs.some((tab) =>
      tab.id === disabledChild
    ),
    "replayed tab closure flushes after enabled incoming completion",
  );
  completed.push("disabled tombstones retain local content until replay");

  const disabledApplyParent = uuid();
  let enteredDisabledApply, releaseDisabledApply;
  const disabledApplyEntered = new Promise((resolve) => {
    enteredDisabledApply = resolve;
  });
  const disabledApplyRelease = new Promise((resolve) => {
    releaseDisabledApply = resolve;
  });
  nextApplyGate = {
    entered: enteredDisabledApply,
    release: disabledApplyRelease,
  };
  const disabledApplyRecords = [
    record(disabledApplyParent, metaEntity(disabledApplyParent)),
  ];
  const pendingDisabledApply = engine._applyRecords(
    disabledApplyRecords,
    incomingTelemetry,
  );
  await disabledApplyEntered;
  Services.prefs.setBoolPref("floorp.workspaces.enabled", false);
  releaseDisabledApply();
  assert(
    (await pendingDisabledApply).includes(disabledApplyParent) &&
      !snapshots.has(disabledApplyParent) &&
      cache.data.pendingApplies.includes(disabledApplyParent),
    "disable aborts an awaited provider apply and retains its desired state",
  );
  await api.capture();
  assert(
    !snapshots.has(disabledApplyParent),
    "ordinary capture cannot retry UI work while disabled",
  );
  Services.prefs.setBoolPref("floorp.workspaces.enabled", true);
  await api.capture();
  assert(
    snapshots.has(disabledApplyParent) &&
      (await engine._applyRecords(disabledApplyRecords, incomingTelemetry))
          .length === 0,
    "re-enable resumes desired UI state and clears replay failures",
  );
  completed.push("disable aborts pending UI work and resumes after re-enable");
  await api.select(workspace, true);
  const bad = uuid();
  snapshots.set(bad, { ...makeSnapshot(bad, []), name: "x".repeat(257) });
  let rejected = false;
  try {
    await api.select(bad, true);
  } catch {
    rejected = true;
  }
  assert(
    rejected && !api.isSelected(bad) && api.errorFor(bad),
    "invalid selection rolls back and exposes an error",
  );
  snapshots.set(workspace, {
    ...snapshots.get(workspace),
    name: "x".repeat(257),
  });
  await api.capture();
  assert(api.errorFor(workspace), "invalid rename pauses only its workspace");
  const good = uuid();
  snapshots.set(good, makeSnapshot(good, []));
  await api.select(good, true);
  assert(
    (await engine._store.getAllIDs())[good],
    "other workspace still syncs while one has invalid metadata",
  );
  snapshots.set(workspace, { ...snapshots.get(workspace), name: "Recovered" });
  await api.capture();
  assert(!api.errorFor(workspace), "corrected metadata resumes sync");
  completed.push("capture errors are isolated, visible and recoverable");
  const firstChildParent = uuid(), firstChild = uuid();
  await engine._store.create(
    record(firstChild, tabEntity(firstChild, firstChildParent)),
  );
  const firstMetadata = metaEntity(firstChildParent);
  firstMetadata.name = "Clock zero";
  firstMetadata.metadata = {
    modifiedAt: 0,
    modifiedBy: "00000000-0000-4000-8000-000000000000",
  };
  await engine._store.create(record(firstChildParent, firstMetadata));
  assert(
    snapshots.get(firstChildParent).name === "Clock zero",
    "first metadata replaces child-first placeholder regardless of clock",
  );
  completed.push("child-first metadata with valid zero clock");
  const tabAfterFailedApply = uuid();
  failNextApply = true;
  const applyFailure = await engine._store.create(
    record(tabAfterFailedApply, tabEntity(tabAfterFailedApply)),
  ).then(() => null, (error) => error);
  assert(
    applyFailure && api.errorFor(workspace) === "incoming-failed",
    "failed incoming UI apply remains pending and visible",
  );
  changeLocalURL("https://example.com/local-after-failed-apply");
  await api.capture();
  assert(
    localURLSurvives("https://example.com/local-after-failed-apply"),
    "retrying a failed apply preserves local navigation made while it was pending",
  );
  assert(
    snapshots.get(workspace).tabs.some((tab) =>
      tab.id === tabAfterFailedApply
    ) &&
      !(await engine._store.createRecord(
        tabAfterFailedApply,
        "floorp-workspaces-v2",
      )).deleted &&
      !api.errorFor(workspace),
    "retry applies the missing remote tab without turning it into a local tombstone and clears the error",
  );
  completed.push(
    "failed UI apply retains remote additions and later local navigation",
  );
  const remoteURLAfterFailure = "https://example.com/remote-after-failed-apply";
  const remoteTabUpdate = tabEntity(tabA);
  remoteTabUpdate.tab.url = remoteURLAfterFailure;
  const knownTabBeforeRemoteUpdate = await engine._store.createRecord(
    tabA,
    "floorp-workspaces-v2",
  );
  remoteTabUpdate.tab.modifiedAt = Math.max(
    Date.now(),
    knownTabBeforeRemoteUpdate.entity.tab.modifiedAt,
  ) + 1000;
  failNextApply = true;
  assert(
    await engine._store.create(record(tabA, remoteTabUpdate)).then(
      () => null,
      (error) => error,
    ),
    "incoming existing-tab update can pause at the failed UI boundary",
  );
  await api.capture();
  assert(
    localURLSurvives(remoteURLAfterFailure),
    "unchanged pre-apply local URL is not misclassified as a new edit on retry",
  );
  const knownTabBeforeFieldUpdate = await engine._store.createRecord(
    tabA,
    "floorp-workspaces-v2",
  );
  const remoteFieldUpdate = tabEntity(tabA);
  remoteFieldUpdate.tab.url = "https://example.com/remote-fields-after-failure";
  remoteFieldUpdate.tab.pinned = true;
  remoteFieldUpdate.tab.modifiedAt = Math.max(
    Date.now(),
    knownTabBeforeFieldUpdate.entity.tab.modifiedAt,
  ) + 1000;
  failNextApply = true;
  assert(
    await engine._store.create(record(tabA, remoteFieldUpdate)).then(
      () => null,
      (error) => error,
    ),
    "incoming URL and pin update can pause before a local title change",
  );
  const beforeLocalTitleChange = snapshots.get(workspace);
  snapshots.set(workspace, {
    ...beforeLocalTitleChange,
    tabs: beforeLocalTitleChange.tabs.map((tab) =>
      tab.id === tabA ? { ...tab, title: "Local title while pending" } : tab
    ),
  });
  await api.capture();
  const mergedFields = snapshots.get(workspace).tabs.find((tab) =>
    tab.id === tabA
  );
  assert(
    mergedFields?.url === remoteFieldUpdate.tab.url && mergedFields.pinned &&
      mergedFields.title === "Local title while pending",
    "local title edit during retry preserves the unrelated remote URL and pin changes",
  );
  completed.push("failed UI retry merges only locally changed tab fields");
  const tabAfterLocalReorder = uuid();
  failNextApply = true;
  assert(
    await engine._store.create(
      record(tabAfterLocalReorder, tabEntity(tabAfterLocalReorder)),
    ).then(() => null, (error) => error),
    "incoming addition can pause before a local tab reorder",
  );
  const beforeLocalReorder = snapshots.get(workspace);
  const reorderableTabs = beforeLocalReorder.tabs.filter((tab) => !tab.pinned);
  assert(reorderableTabs.length >= 2, "reorder fixture has two unpinned tabs");
  const firstReordered = reorderableTabs[0].id;
  const secondReordered = reorderableTabs[1].id;
  const reorderedTabs = [...beforeLocalReorder.tabs];
  const firstIndex = reorderedTabs.findIndex((tab) =>
    tab.id === firstReordered
  );
  const secondIndex = reorderedTabs.findIndex((tab) =>
    tab.id === secondReordered
  );
  [reorderedTabs[firstIndex], reorderedTabs[secondIndex]] = [
    reorderedTabs[secondIndex],
    reorderedTabs[firstIndex],
  ];
  snapshots.set(workspace, { ...beforeLocalReorder, tabs: reorderedTabs });
  await api.capture();
  const recoveredOrder = snapshots.get(workspace).tabs.map((tab) => tab.id);
  assert(
    recoveredOrder.indexOf(secondReordered) <
        recoveredOrder.indexOf(firstReordered) &&
      recoveredOrder.includes(tabAfterLocalReorder) &&
      !(await engine._store.createRecord(
        tabAfterLocalReorder,
        "floorp-workspaces-v2",
      )).deleted,
    "retry preserves the actual local reorder and the unobserved remote addition",
  );
  completed.push(
    "failed UI retry preserves a later local reorder and missing remote tab",
  );
  const tabAfterLocalClose = uuid();
  failNextApply = true;
  assert(
    await engine._store.create(
      record(tabAfterLocalClose, tabEntity(tabAfterLocalClose)),
    ).then(() => null, (error) => error),
    "incoming remote addition can pause before a later local close",
  );
  const beforeLocalClose = snapshots.get(workspace);
  snapshots.set(workspace, {
    ...beforeLocalClose,
    tabs: beforeLocalClose.tabs.filter((tab) => tab.id !== tabA),
  });
  await api.capture();
  assert(
    !snapshots.get(workspace).tabs.some((tab) => tab.id === tabA) &&
      api.isRetired(tabA) &&
      (await engine._store.createRecord(tabA, "floorp-workspaces-v2")).deleted,
    "local close while an apply is pending retires the observed tab instead of reopening it",
  );
  assert(
    snapshots.get(workspace).tabs.some((tab) =>
      tab.id === tabAfterLocalClose
    ) &&
      !(await engine._store.createRecord(
        tabAfterLocalClose,
        "floorp-workspaces-v2",
      )).deleted,
    "missing remote addition survives the same retry that applies a local close",
  );
  completed.push(
    "failed UI retry distinguishes unchanged tabs and later local closes",
  );
  const batchParents = [uuid(), uuid()];
  const batchRecords = batchParents.flatMap((id) => [
    record(id, metaEntity(id)),
    ...Array.from({ length: 30 }, () => {
      const tab = uuid();
      return record(tab, tabEntity(tab, id));
    }),
  ]);
  assert(
    (await engine._applyRecords(batchRecords, {
      addIncomingFailedReason: () => {},
    })).length === 0,
    "multi-workspace native incoming batch applies",
  );
  for (const id of batchParents) {
    assert(
      applyCalls.get(id) === 1 && snapshots.get(id).tabs.length === 30,
      "each changed workspace reaches the UI once per native batch",
    );
  }
  completed.push("native batch coalesces workspace UI mutations");
  const beforeBatchEdit = await engine._store.createRecord(
    tabAfterLocalClose,
    "floorp-workspaces-v2",
  );
  const batchEdit = tabEntity(tabAfterLocalClose);
  batchEdit.tab.url = "https://example.com/remote-update-in-batch";
  batchEdit.tab.pinned = true;
  batchEdit.tab.modifiedAt = beforeBatchEdit.entity.tab.modifiedAt + 1000;
  const batchAddition = uuid();
  const originalCreate = engine._store.create;
  const callsBeforeBatchEdit = applyCalls.get(workspace);
  try {
    engine._store.create = async function (item) {
      await originalCreate.call(this, item);
      if (item.id === tabAfterLocalClose) {
        const current = snapshots.get(workspace);
        snapshots.set(workspace, {
          ...current,
          tabs: current.tabs.map((tab) =>
            tab.id === item.id
              ? { ...tab, title: "Local edit between batch records" }
              : tab
          ),
        });
      }
    };
    assert(
      (await engine._applyRecords([
        record(tabAfterLocalClose, batchEdit),
        record(batchAddition, tabEntity(batchAddition)),
      ], { addIncomingFailedReason: () => {} })).length === 0,
      "native batch merges a local edit between records",
    );
  } finally {
    engine._store.create = originalCreate;
  }
  const afterBatchEdit = snapshots.get(workspace).tabs.find((tab) =>
    tab.id === tabAfterLocalClose
  );
  assert(
    applyCalls.get(workspace) === callsBeforeBatchEdit + 1 &&
      afterBatchEdit.url === batchEdit.tab.url && afterBatchEdit.pinned &&
      afterBatchEdit.title === "Local edit between batch records" &&
      snapshots.get(workspace).tabs.some((tab) => tab.id === batchAddition),
    "coalesced apply keeps unobserved remote fields, later local fields, and additions",
  );
  completed.push("coalesced batch preserves edits between incoming records");
  const failedBatch = [uuid(), uuid()].map((id) => record(id, tabEntity(id)));
  failNextApply = true;
  const failedBatchIDs = await engine._applyRecords(failedBatch, {
    addIncomingFailedReason: () => {},
  });
  assert(
    failedBatchIDs.length === 2 &&
      failedBatch.every((item) =>
        failedBatchIDs.includes(item.id) &&
        cache.data.incomingFailures.includes(item.id)
      ) && api.errorFor(workspace) === "incoming-failed",
    "deferred UI failure reports affected records to native and durable retry state",
  );
  await api.capture();
  assert(
    failedBatch.every((item) =>
      snapshots.get(workspace).tabs.some((tab) => tab.id === item.id)
    ),
    "deferred UI failure retains unobserved additions for recovery",
  );
  assert(
    (await engine._applyRecords(failedBatch, {
          addIncomingFailedReason: () => {},
        })).length === 0 &&
      failedBatch.every((item) =>
        !cache.data.incomingFailures.includes(item.id)
      ),
    "successful batch retry clears only repaired incoming failures",
  );
  completed.push("coalesced UI failure retains native retry bookkeeping");
  const unsubscribedParent = uuid(), unsubscribedChild = uuid();
  await engine._store.create(
    record(unsubscribedParent, metaEntity(unsubscribedParent)),
  );
  await engine._store.create(
    record(unsubscribedChild, tabEntity(unsubscribedChild, unsubscribedParent)),
  );
  await api.select(unsubscribedParent, false);
  await engine._store.remove(tomb(unsubscribedChild));
  await engine._flushIncomingTabs();
  assert(
    api.isPendingClose(unsubscribedChild) &&
      !closed.includes(unsubscribedChild),
    "excluded workspace retains pending remote closure without changing its UI",
  );
  await api.select(unsubscribedParent, true);
  await engine._flushIncomingTabs();
  assert(
    !api.isPendingClose(unsubscribedChild) &&
      closed.includes(unsubscribedChild) &&
      !snapshots.get(unsubscribedParent).tabs.some((tab) =>
        tab.id === unsubscribedChild
      ),
    "reselecting applies and then clears the retained remote closure",
  );
  completed.push("unsubscribe retains closures until selected UI applies");
  const unavailableParent = uuid(), unavailableChild = uuid();
  const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  );
  const originalIsPrivate = PrivateBrowsingUtils.isWindowPrivate;
  unregister();
  try {
    // Simulate the native no-normal-window boundary without opening private
    // browsing content or replacing the test profile's real browser window.
    PrivateBrowsingUtils.isWindowPrivate = () => true;
    for (
      const item of [
        record(unavailableParent, metaEntity(unavailableParent)),
        record(
          unavailableChild,
          tabEntity(unavailableChild, unavailableParent),
        ),
      ]
    ) {
      assert(
        await engine._store.create(item).then(() => false, () => true),
        "incoming UI application cannot succeed with zero normal providers",
      );
    }
    assert(
      api.errorFor(unavailableParent) === "incoming-failed" &&
        (await engine._store.getAllIDs())[unavailableChild] &&
        !api.isRetired(unavailableChild),
      "unapplied remote child stays pending rather than becoming a local closure",
    );
  } finally {
    PrivateBrowsingUtils.isWindowPrivate = originalIsPrivate;
    unregister = await api.register(provider);
  }
  assert(
    snapshots.get(unavailableParent).tabs.some((tab) =>
      tab.id === unavailableChild
    ) &&
      !api.errorFor(unavailableParent),
    "a later normal provider materializes the pending remote workspace and tab",
  );
  completed.push("zero normal providers preserve unobserved remote additions");
  const invalidPendingChild = uuid();
  failNextApply = true;
  assert(
    await engine._store.create(
      record(invalidPendingChild, tabEntity(invalidPendingChild)),
    ).then(() => false, () => true),
    "baseline validation fixture has an unapplied remote addition",
  );
  snapshots.set(workspace, {
    ...snapshots.get(workspace),
    name: "x".repeat(257),
  });
  await api.capture();
  unregister();
  unregister = await api.register(provider);
  assert(
    snapshots.get(workspace).name.length === 257 &&
      !snapshots.get(workspace).tabs.some((tab) =>
        tab.id === invalidPendingChild
      ) &&
      Object.values(cache.data.pendingBaselines).every((value) =>
        value.name.length <= 256
      ),
    "invalid local metadata pauses retry and registration without persisting an invalid baseline or overwriting the UI",
  );
  const invalidPersisted = JSON.parse(JSON.stringify(cache.data));
  invalidPersisted.pendingBaselines[workspace] = snapshots.get(workspace);
  invalidPersisted.pendingBaselines[uuid()] = null;
  const reloadPath = PathUtils.join(
    PathUtils.profileDir,
    "invalid-baseline-reload.json",
  );
  await IOUtils.writeJSON(reloadPath, invalidPersisted);
  const reloaded = new JSONFile({
    path: reloadPath,
    dataPostProcessor: cache._dataPostProcessor,
  });
  await reloaded.load();
  assert(
    !reloaded.data.pendingBaselines[workspace] &&
      reloaded.data.pendingApplies.includes(workspace) &&
      reloaded.data.records[workspace].tabs.some((tab) =>
        tab.id === invalidPendingChild
      ),
    "loading malformed legacy baselines quarantines only observations and retains desired remote additions and pending marker",
  );
  await reloaded.finalize();
  snapshots.set(workspace, {
    ...snapshots.get(workspace),
    name: "Valid after pending error",
  });
  await api.capture();
  assert(
    snapshots.get(workspace).tabs.some((tab) =>
      tab.id === invalidPendingChild
    ) &&
      !api.errorFor(workspace),
    "correcting invalid metadata resumes the retained remote apply",
  );
  completed.push("invalid baseline persistence is isolated and recoverable");

  Services.prefs.setBoolPref("privacy.userContext.enabled", true);
  const { ContextualIdentityService } = ChromeUtils.importESModule(
    "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
  );
  const containerParent = uuid(),
    logicalContainer = uuid(),
    containerTab = uuid();
  const definition = (id) => ({
    version: 2,
    kind: "container",
    containerId: id,
    name: `Workspace QA ${id}`,
    color: "blue",
    icon: "briefcase",
    modifiedAt: Date.now(),
    modifiedBy: device,
  });
  const containerReference = {
    kind: "container",
    containerId: logicalContainer,
  };
  await engine._store.create(
    record(logicalContainer, definition(logicalContainer)),
  );
  const containerMetadata = {
    ...metaEntity(containerParent),
    defaultContainer: containerReference,
  };
  await engine._store.create(record(containerParent, containerMetadata));
  const contained = tabEntity(containerTab, containerParent);
  contained.tab.container = containerReference;
  await engine._store.create(record(containerTab, contained));
  const localBinding = bindings.lookup(containerReference);
  assert(
    localBinding.status === "resolved" && localBinding.userContextId > 0,
    "incoming definition creates a native public container before the tab applies",
  );
  assert(
    ContextualIdentityService.getPublicIdentityFromId(
      localBinding.userContextId,
    ),
    "resolved native identity exists",
  );
  const outgoingDefinition = await engine._store.createRecord(
    logicalContainer,
    "floorp-workspaces-v2",
  );
  const outgoingContained = await engine._store.createRecord(
    containerTab,
    "floorp-workspaces-v2",
  );
  assert(
    outgoingDefinition.entity.kind === "container" &&
      outgoingContained.entity.tab.container.containerId === logicalContainer &&
      !JSON.stringify(outgoingDefinition.entity).includes("userContextId"),
    "definition and tab wire reference retain logical identity without native numeric IDs",
  );
  const noneTab = uuid();
  await engine._store.create(
    record(noneTab, tabEntity(noneTab, containerParent)),
  );
  assert(
    (await engine._store.createRecord(noneTab, "floorp-workspaces-v2")).entity
          .tab.container.kind === "none" &&
      bindings.lookup({ kind: "none" }).userContextId === 0,
    "explicit none remains zero even inside a workspace with a default container",
  );
  completed.push(
    "native v2 container definition, tab reference and explicit none",
  );

  const reboundParent = uuid(), reboundTab = uuid();
  const originalNative = ContextualIdentityService.create(
    `QA original ${reboundParent}`,
    "briefcase",
    "blue",
  );
  const replacementNative = ContextualIdentityService.create(
    `QA replacement ${reboundParent}`,
    "fingerprint",
    "red",
  );
  await ContextualIdentityService.save();
  const canonicalRebound =
    bindings.captureReference(originalNative.userContextId).ref;
  const reboundSnapshot = makeSnapshot(reboundParent, [reboundTab]);
  reboundSnapshot.defaultContainer = canonicalRebound;
  reboundSnapshot.tabs[0].container = canonicalRebound;
  snapshots.set(reboundParent, reboundSnapshot);
  await api.select(reboundParent, true);
  await api.capture();
  let markedBeforeCapture = false, durableBeforeApply = false;
  const localRebindURL = "https://example.com/local-navigation-during-rebind";
  snapshots.set(reboundParent, {
    ...snapshots.get(reboundParent),
    tabs: snapshots.get(reboundParent).tabs.map((tab) => ({
      ...tab,
      url: localRebindURL,
    })),
  });
  beforeCaptureCheck = (id) => {
    if (id !== reboundParent || markedBeforeCapture) return;
    assert(
      api.isContainerDefaultPending(id) && api.isContainerApplyPending(id),
      "association changes mark canonical defaults before provider recapture can reinterpret old native state",
    );
    markedBeforeCapture = true;
  };
  beforeApplyCheck = async (document) => {
    if (document.workspaceId !== reboundParent || durableBeforeApply) return;
    const persisted = await IOUtils.readJSON(cache.path);
    assert(
      persisted.containerPending[reboundParent]?.defaultContainer &&
        persisted.containerPending[reboundParent]?.tabs.includes(reboundTab),
      "association pending state is durable before provider application",
    );
    durableBeforeApply = true;
  };
  try {
    assert(
      canonicalRebound.kind === "container",
      "rebind fixture has a logical identity",
    );
    await bindings.bind(
      canonicalRebound.containerId,
      replacementNative.userContextId,
    );
    await api.capture();
    assert(
      markedBeforeCapture && durableBeforeApply &&
        api.getSyncedWorkspace(reboundParent).defaultContainer.containerId ===
          canonicalRebound.containerId &&
        api.getSyncedTab(reboundParent, reboundTab).url === localRebindURL,
      "explicit reassociation preserves the canonical default and a concurrent local navigation",
    );
  } finally {
    beforeCaptureCheck = null;
    beforeApplyCheck = null;
  }
  completed.push(
    "association rebind durably protects canonical defaults before capture and preserves concurrent local navigation",
  );

  const missingParent = uuid(), missingTab = uuid(), missingContainer = uuid();
  await engine._store.create(record(missingParent, metaEntity(missingParent)));
  const missingEntity = tabEntity(missingTab, missingParent);
  missingEntity.tab.container = {
    kind: "container",
    containerId: missingContainer,
  };
  await engine._store.create(record(missingTab, missingEntity));
  await api.capture();
  assert(
    api.isContainerApplyPending(missingParent) &&
      !snapshots.get(missingParent).tabs.some((tab) => tab.id === missingTab) &&
      !(await engine._store.createRecord(missingTab, "floorp-workspaces-v2"))
        .deleted &&
      !api.isRetired(missingTab),
    "missing container definition holds the tab without emitting a tombstone",
  );
  await engine._store.create(
    record(missingContainer, definition(missingContainer)),
  );
  await api.capture();
  assert(
    snapshots.get(missingParent).tabs.some((tab) => tab.id === missingTab) &&
      !api.isContainerApplyPending(missingParent),
    "later definition resumes the same desired tab",
  );
  snapshots.set(missingParent, { ...snapshots.get(missingParent), tabs: [] });
  await api.noteTabClosed(missingParent, missingTab);
  await api.capture();
  assert(
    api.isRetired(missingTab) &&
      (await engine._store.createRecord(missingTab, "floorp-workspaces-v2"))
        .deleted,
    "ordinary TabClose still emits a tombstone for a resolved container tab",
  );
  completed.push(
    "pending containers preserve missing tabs and ordinary close still propagates",
  );

  const legacyParent = uuid(), legacyTab = uuid();
  await engine._store.create(record(legacyParent, {
    ...metaEntity(legacyParent),
    defaultContainer: { kind: "legacy-unresolved" },
  }));
  const legacyTabEntity = tabEntity(legacyTab, legacyParent);
  legacyTabEntity.tab.container = { kind: "legacy-unresolved" };
  await engine._store.create(record(legacyTab, legacyTabEntity));
  assert(
    api.isContainerDefaultPending(legacyParent) &&
      !snapshots.get(legacyParent).tabs.some((tab) => tab.id === legacyTab),
    "legacy container uncertainty remains blocked until an explicit choice",
  );
  await api.resolveLegacyContainers(legacyParent, 0, "default");
  assert(
    (await engine._store.createRecord(legacyParent, "floorp-workspaces-v2"))
          .entity.defaultContainer.kind === "none" &&
      (await engine._store.createRecord(legacyTab, "floorp-workspaces-v2"))
          .entity.tab.container.kind === "legacy-unresolved" &&
      !snapshots.get(legacyParent).tabs.some((tab) => tab.id === legacyTab),
    "explicit default none does not silently reinterpret unresolved tab containers",
  );
  await api.resolveLegacyContainers(legacyParent, "new", "tabs");
  // The setup UI retries after making its explicit choices; this also consumes
  // the native creation's association notification before inspecting status.
  await api.capture();
  const resolvedLegacyTab = await engine._store.createRecord(
    legacyTab,
    "floorp-workspaces-v2",
  );
  assert(
    resolvedLegacyTab.entity.tab.container.kind === "container" &&
      bindings.lookup(resolvedLegacyTab.entity.tab.container).status ===
        "resolved" &&
      snapshots.get(legacyParent).tabs.some((tab) => tab.id === legacyTab) &&
      !api.isContainerApplyPending(legacyParent),
    "explicit legacy-tab setup persists a logical reference and creates its native target",
  );
  completed.push(
    "legacy default and tab containers require separate explicit choices and resume safely",
  );

  assert(
    typeof ContextualIdentityService.prepareRemoval === "function",
    "native container lifecycle patch is loaded",
  );
  const removal = await ContextualIdentityService.prepareRemoval(
    localBinding.userContextId,
    "delete",
  );
  const protectedOnDisk = await IOUtils.readJSON(cache.path);
  assert(
    protectedOnDisk.protectedTabs[containerParent]?.includes(containerTab),
    "native preflight durably protects the desired tab before closing it",
  );
  const removalClose = api.noteTabClosed(containerParent, containerTab);
  snapshots.set(containerParent, {
    ...snapshots.get(containerParent),
    tabs: snapshots.get(containerParent).tabs.filter((tab) =>
      tab.id !== containerTab
    ),
  });
  assert(
    ContextualIdentityService.remove(localBinding.userContextId, removal),
    "native prepared deletion succeeds",
  );
  await ContextualIdentityService.finishRemoval(removal, "completed");
  await removalClose;
  await api.capture();
  assert(
    !api.isRetired(containerTab) &&
      !(await engine._store.createRecord(containerTab, "floorp-workspaces-v2"))
        .deleted &&
      bindings.lookup(containerReference).reason === "deleted" &&
      !snapshots.get(containerParent).tabs.some((tab) =>
        tab.id === containerTab
      ),
    "native container deletion leaves its tab desired and blocked rather than uploading a closure",
  );
  completed.push(
    "native container removal persists protection before close and emits no tab tombstone",
  );

  const reopen = await api.beginContainerReopen(containerParent, noneTab);
  const reopeningOnDisk = await IOUtils.readJSON(cache.path);
  assert(
    reopeningOnDisk.reopenOperations[reopen]?.tabId === noneTab,
    "tab replacement intent is durable before closing the source",
  );
  // The adapter excludes its owned programmatic source close from TabClose.
  snapshots.set(containerParent, {
    ...snapshots.get(containerParent),
    tabs: snapshots.get(containerParent).tabs.filter((tab) =>
      tab.id !== noneTab
    ),
  });
  await api.capture();
  assert(
    !api.isRetired(noneTab) &&
      !(await engine._store.createRecord(noneTab, "floorp-workspaces-v2"))
        .deleted,
    "a source absent during programmatic replacement remains desired",
  );
  await api.finishContainerReopen(reopen, false);
  await api.capture();
  assert(
    !cache.data.reopenOperations[reopen] &&
      !(await engine._store.createRecord(noneTab, "floorp-workspaces-v2"))
        .deleted,
    "failed replacement releases its operation while retaining the desired UUID",
  );
  const userCloseDuringReopen = await api.beginContainerReopen(
    containerParent,
    noneTab,
  );
  await api.noteTabClosed(containerParent, noneTab);
  await api.finishContainerReopen(userCloseDuringReopen, false);
  assert(
    (await engine._store.createRecord(noneTab, "floorp-workspaces-v2")).deleted,
    "an explicit user close during replacement emits a tombstone and prevents revival",
  );
  snapshots.set(containerParent, {
    ...snapshots.get(containerParent),
    tabs: [],
  });
  completed.push(
    "durable replacement recovery preserves UUID and releases ordinary close suppression",
  );

  const oldAccountWorkspace = uuid();
  let enteredApply, releaseApply;
  const applyEntered = new Promise((resolve) => {
    enteredApply = resolve;
  });
  const applyRelease = new Promise((resolve) => {
    releaseApply = resolve;
  });
  nextApplyGate = { entered: enteredApply, release: applyRelease };
  const oldIncoming = engine._store.create(
    record(oldAccountWorkspace, metaEntity(oldAccountWorkspace)),
  ).then(() => null, (error) => error);
  await applyEntered;
  Services.obs.notifyObservers(null, "weave:service:start-over");
  releaseApply();
  assert(
    await oldIncoming,
    "old account's in-flight incoming operation is rejected after startOver",
  );
  assert(
    !snapshots.has(oldAccountWorkspace),
    "aborted old-account apply does not update the provider's local content",
  );
  await api.capture();
  const nextEngine = new FloorpWorkspacesEngine(Service);
  await nextEngine.initialize();
  try {
    assert(
      !api.isSelected(workspace) && !api.isSelected(oldAccountWorkspace) &&
        Object.keys(await nextEngine._store.getAllIDs()).length === 0,
      "startOver disarms prior account records and in-flight incoming cannot revive them for the next account session",
    );
    const capturingWorkspace = uuid(), capturingTab = uuid();
    snapshots.set(capturingWorkspace, makeSnapshot(capturingWorkspace, []));
    await api.select(capturingWorkspace, true);
    failNextApply = true;
    assert(
      await nextEngine._store.create(
        record(capturingTab, tabEntity(capturingTab, capturingWorkspace)),
      ).then(() => null, (error) => error),
      "capture reset fixture retains an unapplied remote tab",
    );
    let releaseCapture, enteredCapture;
    const captureEntered = new Promise((resolve) => enteredCapture = resolve);
    const captureRelease = new Promise((resolve) => releaseCapture = resolve);
    nextApplyGate = { entered: enteredCapture, release: captureRelease };
    const staleCapture = api.capture().then(() => null, (error) => error);
    await captureEntered;
    Services.obs.notifyObservers(null, "weave:service:start-over");
    const captureAfterReset = api.capture();
    releaseCapture();
    assert(
      await staleCapture,
      "startOver aborts capture waiting at provider apply",
    );
    await captureAfterReset;
    assert(
      !api.isSelected(capturingWorkspace) &&
        !snapshots.get(capturingWorkspace).tabs.some((tab) =>
          tab.id === capturingTab
        ) &&
        !Object.keys(cache.data.records).length,
      "new capture waits for all durable account clears and cannot resurrect old desired tabs",
    );
    completed.push(
      "startOver fences ongoing capture and subsequent capture waits for durable reset",
    );
  } finally {
    await nextEngine.finalize();
  }
  completed.push(
    "account startOver clears selections and fences in-flight incoming",
  );
  const { getFxAccountsSingleton } = ChromeUtils.importESModule(
    "resource://gre/modules/FxAccounts.sys.mjs",
  );
  const account = getFxAccountsSingleton();
  const originalGetSignedInUser = account.getSignedInUser;
  const originalStartup = SyncEngine.prototype._syncStartup;
  let enteredAccountLookup, releaseAccountLookup;
  const accountLookupEntered = new Promise((resolve) => {
    enteredAccountLookup = resolve;
  });
  const accountLookupRelease = new Promise((resolve) => {
    releaseAccountLookup = resolve;
  });
  let baseStartupCalled = false;
  const lookupEngine = new FloorpWorkspacesEngine(Service);
  await lookupEngine.initialize();
  try {
    account.getSignedInUser = async () => {
      enteredAccountLookup();
      await accountLookupRelease;
      return { uid: uuid().replaceAll("-", "") };
    };
    SyncEngine.prototype._syncStartup = function () {
      baseStartupCalled = true;
      return Promise.resolve();
    };
    const oldStartup = lookupEngine._syncStartup().then(
      () => null,
      (error) => error,
    );
    await accountLookupEntered;
    Services.obs.notifyObservers(null, "weave:service:start-over");
    releaseAccountLookup();
    assert(
      await oldStartup,
      "startup rejects an account lookup that completed after startOver",
    );
    assert(
      !baseStartupCalled,
      "stale account lookup never enters native startup",
    );
    await api.capture();
    // Run the real migration prepare path against scoped native transport
    // fixtures. No production startup or migration bypass is installed.
    const { Collection } = ChromeUtils.importESModule(
      "resource://services-sync/record.sys.mjs",
    );
    const originalGetBatched = Collection.prototype.getBatched;
    const control = new FloorpWorkspaceRecord(
      "floorp-workspaces-migration",
      "e8d1a249-86cb-4464-a898-901d2de7a937",
    );
    control.entity = {
      version: 1,
      kind: "workspace-container-migration",
      migrationId: uuid(),
      ownerId: uuid(),
      phase: "complete",
      sourceModified: 0,
      manifestHash:
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
      recordCount: 0,
    };
    await control.encrypt(key);
    const startupService = Object.create(Service);
    Object.defineProperties(startupService, {
      storageURL: { value: "https://workspace-migration.invalid/storage/" },
      metaURL: {
        value: "https://workspace-migration.invalid/storage/meta/global",
      },
      collectionKeys: { value: { keyForCollection: () => key } },
      recordManager: {
        value: {
          get: () =>
            Promise.resolve({
              modified: 1,
              payload: {
                engines: {
                  "floorp-workspaces-v2": {
                    version: 1,
                    syncID: "qa-v2-sync-id",
                  },
                },
              },
            }),
        },
      },
    });
    let migrationReads = 0;
    Collection.prototype.getBatched = function () {
      assert(
        new URL(this.uri.spec).pathname.endsWith(
          "/floorp-workspaces-migration",
        ),
        "startup reads only the completed migration control collection",
      );
      migrationReads++;
      return Promise.resolve({
        response: {
          success: true,
          status: 200,
          headers: { "x-last-modified": "1" },
        },
        records: [control],
      });
    };
    const freshEngine = new FloorpWorkspacesEngine(startupService);
    await freshEngine.initialize();
    try {
      const epochBeforeFreshStartup = freshEngine.accountSessionEpoch;
      account.getSignedInUser = () =>
        Promise.resolve({
          uid: uuid().replaceAll("-", ""),
        });
      await freshEngine._syncStartup();
      assert(
        migrationReads === 1 && baseStartupCalled &&
          !freshEngine.migrationPending,
        "startup decrypts real native migration control before entering base Sync startup",
      );
      assert(
        freshEngine.accountSessionEpoch === epochBeforeFreshStartup,
        "stale startup did not rebind the old UID after account reset",
      );
      assert(
        Object.keys(await freshEngine._store.getAllIDs()).length === 0,
        "fresh account starts without prior account records",
      );
    } finally {
      Collection.prototype.getBatched = originalGetBatched;
      await freshEngine.finalize();
    }

    Services.obs.notifyObservers(null, "weave:service:start-over");
    await api.ready();
    const startupWorkspace = uuid(), startupTab = uuid();
    snapshots.set(
      startupWorkspace,
      makeSnapshot(startupWorkspace, [startupTab]),
    );
    await api.select(startupWorkspace, true);
    const beforeStartupURL = snapshots.get(startupWorkspace).tabs[0].url;
    const afterStartupURL =
      "https://example.com/new-local-navigation-during-migration";
    const nativeCollections = new Map();
    const collectionClocks = new Map();
    const originalPostQueue = Collection.prototype.newPostQueue;
    let releaseMigrationRead, enteredMigrationRead;
    const migrationReadEntered = new Promise((resolve) =>
      enteredMigrationRead = resolve
    );
    const migrationReadRelease = new Promise((resolve) =>
      releaseMigrationRead = resolve
    );
    let gateFirstRead = true;
    Collection.prototype.getBatched = async function () {
      const collectionName = new URL(this.uri.spec).pathname.split("/").at(-1);
      if (gateFirstRead) {
        gateFirstRead = false;
        assert(
          collectionName === "floorp-workspaces-migration",
          "prepare first reads migration control",
        );
        enteredMigrationRead();
        await migrationReadRelease;
      }
      const values = nativeCollections.get(collectionName);
      return {
        response: {
          success: true,
          status: values ? 200 : 404,
          headers: {
            "x-last-modified": String(
              collectionClocks.get(collectionName) ?? 0,
            ),
          },
        },
        records: [...(values?.values() ?? [])].map((json) => {
          const value = new FloorpWorkspaceRecord(collectionName, json.id);
          value.deserialize(structuredClone(json));
          return value;
        }),
      };
    };
    Collection.prototype.newPostQueue = function (
      _log,
      conditionalModified,
      callback,
    ) {
      const collectionName = new URL(this.uri.spec).pathname.split("/").at(-1);
      assert(
        collectionName === "floorp-workspaces-migration",
        "migration never writes the legacy collection",
      );
      const queued = [];
      return {
        enqueue(value) {
          assert(
            value.ciphertext && value.cleartext === null,
            "migration control uses native encrypted records",
          );
          queued.push(value.toJSON());
          return Promise.resolve({ enqueued: true });
        },
        flush() {
          assert(
            conditionalModified === (collectionClocks.get(collectionName) ?? 0),
            "native migration write keeps its conditional server version",
          );
          const values = nativeCollections.get(collectionName) ?? new Map();
          for (const value of queued) {
            values.set(value.id, structuredClone(value));
          }
          nativeCollections.set(collectionName, values);
          collectionClocks.set(collectionName, conditionalModified + 1);
          callback(null, {
            success: true,
            status: 200,
            obj: { success: queued.map((value) => value.id), failed: {} },
            headers: {},
          }, false);
          return Promise.resolve();
        },
      };
    };
    const pendingEngine = new FloorpWorkspacesEngine(startupService);
    await pendingEngine.initialize();
    try {
      const startupUid = uuid().replaceAll("-", "");
      account.getSignedInUser = () => Promise.resolve({ uid: startupUid });
      const startupPending = pendingEngine._syncStartup();
      await migrationReadEntered;
      await new Promise((resolve) => win.setTimeout(resolve, 5));
      snapshots.set(startupWorkspace, {
        ...snapshots.get(startupWorkspace),
        tabs: snapshots.get(startupWorkspace).tabs.map((tab) => ({
          ...tab,
          url: afterStartupURL,
        })),
      });
      await api.capture();
      releaseMigrationRead();
      await startupPending;
      assert(
        (await IOUtils.readJSON(cache.path)).migrationResendPending,
        "migration persists its current-state resend obligation before native startup",
      );
      assert(
        pendingEngine.migrationPending &&
          api.getSyncedTab(startupWorkspace, startupTab)?.url ===
            afterStartupURL,
        "startup preserves a newer local navigation captured while the migration snapshot was awaiting transport",
      );
      const stagedIDs = Object.keys(await pendingEngine._store.getAllIDs());
      const uploaded = new Map();
      for (const id of stagedIDs) {
        const staged = await pendingEngine._store.createRecord(
          id,
          "floorp-workspaces-v2",
        );
        if (id === startupTab) {
          assert(
            staged.entity.tab.url === beforeStartupURL,
            "migration outgoing payload stays exactly equal to its immutable staged manifest",
          );
        }
        await staged.encrypt(key);
        uploaded.set(id, staged.toJSON());
      }
      nativeCollections.set("floorp-workspaces-v2", uploaded);
      collectionClocks.set("floorp-workspaces-v2", 1);
      await pendingEngine._onRecordsWritten(stagedIDs);
      assert(
        !pendingEngine.migrationPending &&
          pendingEngine._modified.ids().includes(startupTab) &&
          (await pendingEngine._store.createRecord(
              startupTab,
              "floorp-workspaces-v2",
            )).entity.tab.url === afterStartupURL,
        "acknowledged migration completion retracks the newer current payload for a subsequent upload",
      );
      const persistedResend = await IOUtils.readJSON(cache.path);
      assert(
        persistedResend.migrationResendPending &&
          cache._dataPostProcessor(persistedResend).migrationResendPending,
        "staged ACK does not clear the durable resend marker when a newer local payload remains",
      );
      const originalTrackedIDs = pendingEngine._tracker.getChangedIDs;
      try {
        pendingEngine._tracker.getChangedIDs = () => Promise.resolve({});
        const resendIDs = Object.keys(await pendingEngine.getChangedIDs());
        assert(
          resendIDs.includes(startupTab),
          "an empty native tracker still resends the current edit from its durable migration marker",
        );
        for (const id of resendIDs) {
          await pendingEngine._store.createRecord(id, "floorp-workspaces-v2");
        }
        await pendingEngine._onRecordsWritten(resendIDs);
        assert(
          !(await IOUtils.readJSON(cache.path)).migrationResendPending,
          "only acknowledgement of the exact current payload clears the resend obligation",
        );
      } finally {
        pendingEngine._tracker.getChangedIDs = originalTrackedIDs;
      }
      completed.push(
        "migration startup retains local edits, stages exact uploads and durably resends current edits with an empty tracker",
      );
    } finally {
      Collection.prototype.getBatched = originalGetBatched;
      Collection.prototype.newPostQueue = originalPostQueue;
      await pendingEngine.finalize();
    }
  } finally {
    account.getSignedInUser = originalGetSignedInUser;
    SyncEngine.prototype._syncStartup = originalStartup;
    await lookupEngine.finalize();
  }
  completed.push(
    "account lookup resolving after startOver cannot rebind the old UID",
  );
  await engine._tracker.stop();
  await engine.finalize();
  unregister();
  return { passed: true, cases: completed };
}
