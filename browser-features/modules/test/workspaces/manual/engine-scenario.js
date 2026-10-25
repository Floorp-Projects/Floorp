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
  const workspace = uuid(), tabA = uuid(), tabB = uuid(), device = uuid();
  const snapshots = new Map();
  const closed = [];
  const applyCalls = new Map();
  let nextApplyGate = null;
  let failNextApply = false;
  const makeSnapshot = (id, tabs) => ({
    workspaceId: id,
    name: "Research",
    icon: "book",
    tabs: tabs.map((id) => ({
      id,
      url: "https://example.com/" + id,
      title: id,
      pinned: false,
    })),
  });
  snapshots.set(workspace, makeSnapshot(workspace, [tabA]));
  const provider = {
    window: win,
    capture: (id) => {
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
    apply: async (document, _missingTabIds, signal) => {
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
      snapshots.set(document.workspaceId, {
        workspaceId: document.workspaceId,
        name: document.name,
        icon: document.icon,
        tabs: model.liveWorkspaceSyncTabs(document).map((
          { id, url, title, pinned },
        ) => ({ id, url, title, pinned })),
      });
    },
  };
  const { JSONFile } = ChromeUtils.importESModule(
    "resource://gre/modules/JSONFile.sys.mjs",
  );
  let cache;
  const originalLoad = JSONFile.prototype.load;
  JSONFile.prototype.load = function (...args) {
    if (
      PathUtils.filename(this.path) === "floorp-workspaces-sync-manual.json"
    ) {
      cache = this;
    }
    return originalLoad.apply(this, args);
  };
  let unregister;
  try {
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
  Services.prefs.setBoolPref("services.sync.engine.floorp-workspaces", true);
  engine._tracker.start();
  const record = (id, entity) => {
    const result = new FloorpWorkspaceRecord("floorp-workspaces", id);
    result.entity = entity;
    return result;
  };
  const tabEntity = (id, parent = workspace) => ({
    version: 1,
    kind: "tab",
    workspaceId: parent,
    tab: {
      id,
      url: "https://example.com/" + id,
      title: id,
      pinned: false,
      position: 1,
      deleted: false,
      modifiedAt: Date.now() + 1000,
      modifiedBy: device,
    },
  });
  const metaEntity = (id) => ({
    version: 1,
    kind: "workspace",
    workspaceId: id,
    name: "Remote",
    icon: null,
    metadata: { modifiedAt: Date.now() + 1000, modifiedBy: device },
  });
  const tomb = (id) => {
    const result = new FloorpWorkspaceRecord("floorp-workspaces", id);
    result.deleted = true;
    return result;
  };
  const completed = [];
  await api.select(workspace, true);
  const ids = await engine._store.getAllIDs();
  assert(
    Object.keys(ids).length === 2 && ids[workspace] && ids[tabA],
    "selected workspace emits metadata and separate tab records",
  );
  const outgoing = await engine._store.createRecord(tabA, "floorp-workspaces");
  assert(
    outgoing.entity.kind === "tab" && !Object.hasOwn(outgoing.entity, "tabs"),
    "tab payload is independent",
  );
  const { BulkKeyBundle } = ChromeUtils.importESModule(
    "resource://services-sync/keys.sys.mjs",
  );
  const key = new BulkKeyBundle("floorp-workspaces");
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
    (await engine._store.createRecord(tabB, "floorp-workspaces")).deleted,
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
  assert(!engine.enabled, "disabled workspaces disable engine");
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
    await engine._store.createRecord(tabA, "floorp-workspaces");
    outgoingWhileDisabled = true;
  } catch { /* A previously started native upload may not read more tabs. */ }
  assert(!outgoingWhileDisabled, "disabled outgoing reads reject");
  Services.prefs.setStringPref(
    "services.sync.floorp-workspaces.lastSync",
    "12345",
  );
  Services.prefs.setBoolPref("floorp.workspaces.enabled", true);
  await api.capture();
  assert(
    Services.prefs.getStringPref("services.sync.floorp-workspaces.lastSync") ===
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
        "floorp-workspaces",
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
    "floorp-workspaces",
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
    "floorp-workspaces",
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
        "floorp-workspaces",
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
      (await engine._store.createRecord(tabA, "floorp-workspaces")).deleted,
    "local close while an apply is pending retires the observed tab instead of reopening it",
  );
  assert(
    snapshots.get(workspace).tabs.some((tab) =>
      tab.id === tabAfterLocalClose
    ) &&
      !(await engine._store.createRecord(
        tabAfterLocalClose,
        "floorp-workspaces",
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
    "floorp-workspaces",
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
    const freshEngine = new FloorpWorkspacesEngine(Service);
    await freshEngine.initialize();
    try {
      const epochBeforeFreshStartup = freshEngine.accountSessionEpoch;
      account.getSignedInUser = () =>
        Promise.resolve({
          uid: uuid().replaceAll("-", ""),
        });
      await freshEngine._syncStartup();
      assert(
        freshEngine.accountSessionEpoch === epochBeforeFreshStartup,
        "stale startup did not rebind the old UID after account reset",
      );
      assert(
        Object.keys(await freshEngine._store.getAllIDs()).length === 0,
        "fresh account starts without prior account records",
      );
    } finally {
      await freshEngine.finalize();
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
