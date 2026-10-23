// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  parseWorkspaceBookmarkState,
  type WorkspaceBookmarkItem,
  WorkspaceBookmarksService,
  type WorkspaceBookmarkState,
  type WorkspaceBookmarkTree,
} from "../WorkspaceBookmarks.sys.mts";
import {
  assert,
  assertEquals,
  assertThrows,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

const A = "11111111-1111-4111-8111-111111111111";
const B = "22222222-2222-4222-8222-222222222222";
const C = "33333333-3333-4333-8333-333333333333";
const SOURCE = "44444444-4444-4444-8444-444444444444";
const URL = "https://example.com/shared";

function clone<T>(value: T): T {
  return structuredClone(value);
}

function equalJSON(actual: unknown, expected: unknown, message: string): void {
  assertEquals(JSON.stringify(actual), JSON.stringify(expected), message);
}

async function rejects(
  operation: Promise<unknown>,
  message: string,
): Promise<void> {
  let rejected = false;
  try {
    await operation;
  } catch {
    rejected = true;
  }
  assert(rejected, message);
}

function state(rules: Record<string, string[]> = {}): WorkspaceBookmarkState {
  return { schemaVersion: 1, sourceId: SOURCE, rules, workspaceFolders: {} };
}

class MemoryStorage {
  value: unknown = state();
  recovery: unknown | null = null;
  failWrites = false;
  beforeWrite: (() => Promise<void>) | null = null;

  read(): Promise<unknown> {
    return Promise.resolve(clone(this.value));
  }

  readRecovery(): Promise<unknown | null> {
    return Promise.resolve(clone(this.recovery));
  }

  async write(
    next: WorkspaceBookmarkState,
    preserveLastGood: boolean,
  ): Promise<void> {
    await this.beforeWrite?.();
    if (this.failWrites) throw new Error("Simulated disk write failure");
    if (preserveLastGood) this.recovery = clone(this.value);
    this.value = clone(next);
  }
}

type Change = { type: string; id?: number; guid?: string; oldGuid?: string };
class FakeChanges {
  listener: ((changes: Change[]) => void) | null = null;
  addListener(_events: string[], listener: (changes: Change[]) => void): void {
    this.listener = listener;
  }
  removeListener(): void {
    this.listener = null;
  }
  emit(changes: Change[]): void {
    this.listener?.(changes);
  }
}

function initialTree(): WorkspaceBookmarkTree {
  return {
    guid: "root________",
    typeCode: 2,
    children: [{
      guid: "unfiled_____",
      typeCode: 2,
      title: "Other",
      children: [
        {
          guid: "alpha_______",
          id: 10,
          typeCode: 2,
          title: "Same name",
          children: [
            {
              guid: "bookmarkA___",
              id: 11,
              typeCode: 1,
              title: "Work title",
              uri: URL,
            },
            {
              guid: "nested______",
              id: 12,
              typeCode: 2,
              children: [
                {
                  guid: "impossible__",
                  id: 13,
                  typeCode: 1,
                  uri: "https://example.com/intersection",
                },
              ],
            },
          ],
        },
        {
          guid: "beta________",
          id: 20,
          typeCode: 2,
          title: "Same name",
          children: [
            {
              guid: "bookmarkB___",
              id: 21,
              typeCode: 1,
              title: "Personal title",
              uri: URL,
            },
          ],
        },
        { guid: "hidden______", id: 30, typeCode: 2, children: [] },
        {
          guid: "common______",
          id: 40,
          typeCode: 1,
          title: "Common",
          uri: "https://example.com/common",
        },
      ],
    }],
  };
}

class FakePlaces {
  tree = initialTree();
  nextGuid = 0;
  insertError = false;
  beforeInsert: ((guid: string) => void) | null = null;
  beforeRemove: ((guid: string) => void) | null = null;
  nextTree: (() => Promise<WorkspaceBookmarkTree>) | null = null;
  history = {
    makeGuid: (): string => `new${String(++this.nextGuid).padStart(9, "0")}`,
  };
  bookmarks = {
    rootGuid: "root________",
    unfiledGuid: "unfiled_____",
    TYPE_BOOKMARK: 1,
    TYPE_FOLDER: 2,
    fetch: (
      input: string | { guid: string },
    ): Promise<WorkspaceBookmarkItem | null> => {
      const found = this.find(typeof input === "string" ? input : input.guid);
      return Promise.resolve(
        found
          ? {
            guid: found.node.guid,
            parentGuid: found.parent?.guid ?? null,
            type: found.node.typeCode ?? 1,
            title: found.node.title ?? "",
            url: found.node.uri ?? null,
          }
          : null,
      );
    },
    insert: async (
      input: { guid?: string; parentGuid: string; type: number; title: string },
    ): Promise<WorkspaceBookmarkItem> => {
      const guid = input.guid ?? this.history.makeGuid();
      this.beforeInsert?.(guid);
      if (this.insertError) {
        throw new Error("Simulated Places insertion failure");
      }
      const parent = this.find(input.parentGuid)?.node;
      if (!parent) throw new Error("Missing parent");
      (parent.children ??= []).push({
        guid,
        title: input.title,
        typeCode: input.type,
        children: [],
      });
      return (await this.bookmarks.fetch(guid))!;
    },
    remove: (
      guid: string,
      options?: { preventRemovalOfNonEmptyFolders?: boolean },
    ): Promise<void> => {
      this.beforeRemove?.(guid);
      const found = this.find(guid);
      if (!found?.parent?.children) return Promise.resolve();
      if (
        options?.preventRemovalOfNonEmptyFolders &&
        (found.node.children?.length ?? 0) > 0
      ) return Promise.reject(new Error("Cannot remove a non-empty folder"));
      found.parent.children = found.parent.children.filter((item) =>
        item.guid !== guid
      );
      return Promise.resolve();
    },
  };

  find(
    guid: string,
    node = this.tree,
    parent: WorkspaceBookmarkTree | null = null,
  ):
    | { node: WorkspaceBookmarkTree; parent: WorkspaceBookmarkTree | null }
    | null {
    if (node.guid === guid) return { node, parent };
    for (const child of node.children ?? []) {
      const found = this.find(guid, child, node);
      if (found) return found;
    }
    return null;
  }

  async promiseBookmarksTree(): Promise<WorkspaceBookmarkTree> {
    const pending = this.nextTree;
    this.nextTree = null;
    return pending ? await pending() : clone(this.tree);
  }
}

function fakeWindow(windowType = "navigator:browser"): Window {
  return {
    closed: false,
    document: { documentElement: { getAttribute: () => windowType } },
  } as unknown as Window;
}

async function fixture(
  options: { storage?: MemoryStorage; observe?: boolean; places?: FakePlaces } =
    {},
) {
  const storage = options.storage ?? new MemoryStorage();
  const places = options.places ?? new FakePlaces();
  const changes = new FakeChanges();
  let enabled = true;
  const service = new WorkspaceBookmarksService({
    storage,
    places,
    changes,
    observe: options.observe ?? false,
    sourceId: SOURCE,
    readEnabled: () => enabled,
    writeEnabled: (next) => {
      enabled = next;
    },
    knownWorkspaces: () => [A, B, C],
  });
  const a = fakeWindow();
  const b = fakeWindow();
  service.setWindowContext(a, A, true);
  service.setWindowContext(b, B, true);
  await service.ready;
  return { service, storage, places, changes, a, b };
}

async function testIdentityVisibilityAndIntersections(): Promise<void> {
  const { service, a, b } = await fixture();
  try {
    await service.setFolderScope("alpha_______", [A]);
    await service.setFolderScope("beta________", [B]);
    await service.setFolderScope("nested______", [B]);
    await service.setFolderScope("hidden______", []);
    equalJSON(
      (await service.visibleBookmarksForUrl(URL, a)).map((item) => item.title),
      ["Work title"],
      "visible metadata comes from the eligible GUID",
    );
    equalJSON(
      (await service.visibleBookmarksForUrl(URL, b)).map((item) => item.title),
      ["Personal title"],
      "same URL does not share scoped titles",
    );
    assert(
      service.isUrlVisible(URL, a),
      "URL-shared tags and keywords are eligible when one GUID is visible",
    );
    assert(
      !service.isUrlVisible(URL, null),
      "unknown windows only see common bookmarks",
    );
    assert(
      !service.isVisible("impossible__", a) &&
        !service.isVisible("impossible__", b),
      "a child cannot widen its parent's scope",
    );
    equalJSON(
      service.getEffectiveScope("nested______").workspaceIds,
      [],
      "conflicting ancestor scopes intersect to empty",
    );
    assert(
      !service.isVisible("hidden______", a),
      "explicit empty scope is not common",
    );
    assert(
      service.isVisible("common______", null),
      "unscoped bookmarks remain common",
    );
    assert(
      service.isVisible(
        "bookmarkB___",
        service.getContext(a, { allItems: true }),
      ),
      "explicit management context exposes all items",
    );
    assert(
      service.isVisible("bookmarkA___", fakeWindow("Places:Organizer")),
      "native bookmark manager has an all-items context",
    );
    const sidebar = {
      browsingContext: { topChromeWindow: a },
    } as unknown as Window;
    assertEquals(
      service.getContext(sidebar).windowId,
      service.getContext(a).windowId,
      "sidebar uses its browser window context",
    );
    const dialog = {
      document: {
        documentURI: "chrome://browser/content/places/bookmarkProperties.xhtml",
      },
      opener: a,
    } as unknown as Window;
    assertEquals(
      service.getContext(dialog).windowId,
      service.getContext(a).windowId,
      "bookmark properties uses its owning browser context",
    );
    assertThrows(
      () => parseWorkspaceBookmarkState(state({ root________: [A] })),
      "built-in root rules cannot hide the whole bookmark system",
    );
  } finally {
    service.destroy();
  }
}

async function testWindowSnapshotsAndAtomicFailure(): Promise<void> {
  const { service, storage, a, b } = await fixture();
  try {
    await service.setFolderScope("alpha_______", [A]);
    const snapshotA = service.getContext(a);
    const snapshotB = service.getContext(b);
    const sameWorkspaceWindow = fakeWindow();
    service.setWindowContext(sameWorkspaceWindow, A, true);
    assert(
      !service.isCurrentContext(snapshotA, sameWorkspaceWindow),
      "a matching workspace in another window cannot reuse a captured window context",
    );
    service.setWindowContext(a, B, true);
    assert(
      !service.isCurrentContext(snapshotA),
      "switched window invalidates its asynchronous work",
    );
    assert(
      service.isCurrentContext(snapshotB),
      "other windows keep their own session selection",
    );
    assert(
      !service.isVisible("common______", snapshotA),
      "stale snapshots fail closed even for common items",
    );
    service.setWindowContext(a, A, true);
    storage.failWrites = true;
    await rejects(
      service.setFolderScope("alpha_______", [B]),
      "a failed atomic write is reported",
    );
    equalJSON(
      service.getScope("alpha_______"),
      [A],
      "failed writes preserve the last committed view",
    );
    storage.failWrites = false;
    await service.setFolderScope("alpha_______", [A, B]);
    assert(
      service.isVisible("bookmarkA___", b),
      "the mutation queue recovers after a write failure",
    );
    service.setEnabled(false);
    const disabledSnapshot = service.getContext(a);
    assert(
      service.isVisible("impossible__", a),
      "disabling scopes restores the ordinary bookmark view",
    );
    service.setEnabled(true);
    assert(
      !service.isVisible("impossible__", disabledSnapshot),
      "a disabled snapshot cannot bypass scopes after they are enabled",
    );
    assertEquals(
      service.getVisibleBookmarkGuidsJSON(disabledSnapshot),
      "[]",
      "a stale disabled SQL context fails closed",
    );
  } finally {
    service.destroy();
  }
}

async function testCorruptionRecoveryAndImport(): Promise<void> {
  const storage = new MemoryStorage();
  storage.value = { broken: true };
  const deletedWorkspace = "77777777-7777-4777-8777-777777777777";
  storage.recovery = state({
    alpha_______: [A],
    hidden______: [deletedWorkspace],
    orphan______: [deletedWorkspace],
  });
  const { service, a } = await fixture({ storage });
  try {
    assertEquals(
      service.getStatus().state,
      "error",
      "corrupt settings have an explicit recovery state",
    );
    assert(
      !service.isVisible("common______", a),
      "corruption cannot silently reveal scoped items",
    );
    assert(
      service.isVisible(
        "bookmarkA___",
        service.getContext(a, { allItems: true }),
      ),
      "management remains available for recovery",
    );
    const recovery = await service.readRecoverySettings();
    assert(recovery, "last good settings can be explicitly recovered");
    const foreign = { ...recovery, sourceId: C };
    await rejects(
      service.applyImportSettings(await service.previewImportSettings(foreign)),
      "an unresolved foreign backup cannot release a corrupt store's hold",
    );
    await service.applyImportSettings(
      await service.previewImportSettings(recovery),
    );
    assertEquals(
      service.getStatus().state,
      "ready",
      "validated recovery restores the service",
    );
    assert(
      !service.isVisible("bookmarkA___", null),
      "recovered scope stays local to the original workspace",
    );
    equalJSON(
      service.getScope("hidden______"),
      [deletedWorkspace],
      "exact local last-good recovery preserves deleted workspace references",
    );
    equalJSON(
      service.getScope("orphan______"),
      [deletedWorkspace],
      "exact local last-good recovery preserves removed-folder tombstones",
    );
    assert(
      !service.isVisible("hidden______", a),
      "a deleted workspace reference cannot become common during recovery",
    );
    const backup = await service.exportSettings({
      workspaceNames: { [A]: "Same name" },
    });
    delete backup.rules.hidden______;
    delete backup.rules.orphan______;
    backup.rules.beta________ = ["99999999-9999-4999-8999-999999999999"];
    backup.rules.missing_____ = [A, "88888888-8888-4888-8888-888888888888"];
    const preview = await service.previewImportSettings(backup);
    equalJSON(
      preview.unresolvedFolders,
      ["missing_____"],
      "deleted GUIDs are never rebound by title",
    );
    assertEquals(
      preview.unresolvedWorkspaces.length,
      2,
      "workspace references are reported even when their folder also needs mapping",
    );
    await service.applyImportSettings(preview);
    assertEquals(
      service.getScope("beta________"),
      null,
      "unresolved rules are not applied as common scopes",
    );
    await rejects(
      service.applyImportSettings(preview),
      "committed changes invalidate an earlier import preview",
    );
    await service.applyImportSettings(
      await service.previewImportSettings(backup),
      {
        workspaces: { "99999999-9999-4999-8999-999999999999": B },
      },
    );
    equalJSON(
      service.getScope("beta________"),
      [B],
      "an explicit workspace mapping resolves the rule",
    );
    const stale = await service.previewImportSettings(backup);
    await service.setFolderScope("alpha_______", [C]);
    await rejects(
      service.applyImportSettings(stale),
      "a preview cannot overwrite scope edits committed while confirmation was open",
    );
    equalJSON(
      service.getScope("alpha_______"),
      [C],
      "stale preview rejection preserves the newer edit",
    );
    await rejects(
      service.previewImportSettings({ ...backup, schemaVersion: 99 }),
      "unknown backup versions cannot overwrite the store",
    );
  } finally {
    service.destroy();
  }
}

async function testWorkspaceRestoreAndSaveDestinations(): Promise<void> {
  const { service, storage, places, a } = await fixture();
  try {
    await service.setFolderScope("alpha_______", [A, B]);
    await service.reassignWorkspace(A, C);
    equalJSON(
      service.getScope("alpha_______"),
      [B, C],
      "restore only remaps the archived stable ID and retains later scope edits",
    );
    service.setWindowContext(a, C, true);
    places.beforeInsert = (guid) => {
      const saved = parseWorkspaceBookmarkState(storage.value);
      equalJSON(
        saved.rules[guid],
        [C],
        "a new dedicated folder is scoped before Places exposes its GUID",
      );
    };
    const created = await service.getOrCreateWorkspaceFolder(a, "Work");
    assert(
      created.created,
      "the first explicit save creates a dedicated folder",
    );
    const reused = await service.getOrCreateWorkspaceFolder(
      a,
      "Renamed workspace",
    );
    assertEquals(
      reused.guid,
      created.guid,
      "the stable GUID is reused without title inference",
    );
    assert(!reused.created, "a second save does not duplicate the folder");
    service.rememberDefaultFolder(a, created.guid);
    assertEquals(
      await service.getDefaultFolder(a, "unfiled_____"),
      created.guid,
      "a workspace's remembered folder wins over the global default",
    );
    service.setWindowContext(a, B, true);
    assertEquals(
      await service.getDefaultFolder(a, created.guid),
      "unfiled_____",
      "an invisible folder is not inherited from another workspace's default",
    );
    service.setWindowContext(a, C, true);
    await service.releaseCreatedWorkspaceFolder(created);
    assert(
      !places.find(created.guid),
      "cancelling an unused dedicated folder removes it",
    );
    places.insertError = true;
    await rejects(
      service.getOrCreateWorkspaceFolder(a, "Fail"),
      "native insert failures are reported",
    );
    assertEquals(
      Object.keys(parseWorkspaceBookmarkState(storage.value).workspaceFolders)
        .length,
      0,
      "failed insertion rolls back the reserved folder reference",
    );
    places.insertError = false;
    let insertsAfterSwitch = 0;
    places.beforeInsert = () => {
      insertsAfterSwitch++;
    };
    storage.beforeWrite = () => {
      service.setWindowContext(a, B, true);
      service.setWindowContext(a, C, true);
      return Promise.resolve();
    };
    await rejects(
      service.getOrCreateWorkspaceFolder(a, "Stale save"),
      "leaving and returning to a workspace invalidates the pending save",
    );
    assertEquals(
      insertsAfterSwitch,
      0,
      "an A-to-B-to-A switch cannot commit a stale save",
    );
    storage.beforeWrite = null;
  } finally {
    service.destroy();
  }
}

async function testTransfersPreserveSubtreeRules(): Promise<void> {
  const { service, storage, places, a } = await fixture();
  try {
    await service.setFolderScope("alpha_______", [A]);
    await service.setFolderScope("nested______", [B]);
    const external = await service.captureTransferScopes(
      [{ itemGuid: "" }, { itemGuid: "foreign_____" }],
      "unfiled_____",
      true,
      service.getContext(a),
    );
    assertEquals(
      external.sourceGuids.length,
      0,
      "external URL drags inherit the destination instead of guessing a source",
    );
    const plan = await service.captureTransferScopes(
      ["alpha_______"],
      "unfiled_____",
      true,
      service.getContext(a),
    );
    assertEquals(
      plan.hiddenDescendantCount,
      2,
      "copy confirmation counts hidden descendants",
    );
    await service.beginTransferScopes(plan);
    assert(
      parseWorkspaceBookmarkState(storage.value).pendingCopy,
      "copy reservation is durable before native insertion",
    );
    assert(
      !service.getContext(a).ready,
      "native copies stay unavailable until their rules are persisted",
    );
    const copy = clone(places.find("alpha_______")!.node);
    copy.guid = "copyRoot____";
    copy.children![0].guid = "copyBookmark";
    copy.children![1].guid = "copyNested__";
    copy.children![1].children![0].guid = "copyHidden__";
    places.find("unfiled_____")!.node.children!.push(copy);
    await service.applyTransferScopes(plan, { alpha_______: copy.guid });
    assert(
      !parseWorkspaceBookmarkState(storage.value).pendingCopy,
      "copy rules and marker clearance commit together",
    );
    equalJSON(
      service.getScope("copyRoot____"),
      [A],
      "root rules follow the transaction's explicit GUID mapping",
    );
    equalJSON(
      service.getScope("copyNested__"),
      [B],
      "descendant rules follow checked subtree positions",
    );
    assert(
      !service.isVisible("copyHidden__", a),
      "the copied child keeps the ancestor intersection",
    );
    const widening = await service.captureTransferScopes(
      ["bookmarkA___"],
      "unfiled_____",
      false,
      service.getContext(a),
    );
    assert(
      widening.requiresConfirmation,
      "moving a child out of a scoped parent warns when visibility expands",
    );
  } finally {
    service.destroy();
  }
}

async function testObserverGuidRemappingAndRefreshRace(): Promise<void> {
  const { service, places, storage, changes, a } = await fixture({
    observe: true,
  });
  try {
    await service.setFolderScope("alpha_______", [A]);
    let releaseWrite: (() => void) | undefined;
    const writeGate = new Promise<void>((resolve) => {
      releaseWrite = resolve;
    });
    storage.beforeWrite = () => writeGate;
    places.find("alpha_______")!.node.guid = "renamed_____";
    changes.emit([{
      type: "bookmark-guid-changed",
      id: 10,
      guid: "renamed_____",
    }]);
    await service.ready;
    assert(
      !service.getContext(a).ready,
      "GUID changes remain closed while the rule remap is being written",
    );
    releaseWrite!();
    storage.beforeWrite = null;
    // Queue a mutation behind the identity update to await its durable commit.
    await service.setFolderScope("beta________", [B]);
    equalJSON(
      service.getScope("renamed_____"),
      [A],
      "native item IDs recover the old GUID without title guessing",
    );
    assertEquals(
      service.getScope("alpha_______"),
      null,
      "the old GUID does not retain a duplicate rule",
    );
    let releaseTree: ((tree: WorkspaceBookmarkTree) => void) | undefined;
    places.nextTree = () =>
      new Promise((resolve) => {
        releaseTree = resolve;
      });
    const staleTree = clone(places.tree);
    const refreshing = service.refresh();
    await Promise.resolve();
    await Promise.resolve();
    places.find("bookmarkA___")!.node.title = "Latest title";
    changes.emit([{
      type: "bookmark-title-changed",
      id: 11,
      guid: "bookmarkA___",
    }]);
    assert(releaseTree, "a tree snapshot is outstanding");
    releaseTree(staleTree);
    await refreshing;
    equalJSON(
      (await service.visibleBookmarksForUrl(URL, a)).map((item) => item.title),
      ["Latest title"],
      "notifications during a tree fetch force a fresh snapshot",
    );
  } finally {
    service.destroy();
  }
}

async function testCopyFailureSurvivesRestart(): Promise<void> {
  const first = await fixture();
  try {
    await first.service.setFolderScope("alpha_______", [A]);
    const stalePlan = await first.service.captureTransferScopes(
      ["alpha_______"],
      "unfiled_____",
      true,
      first.service.getContext(first.a),
    );
    first.storage.beforeWrite = () => {
      first.service.setWindowContext(first.a, B, true);
      first.service.setWindowContext(first.a, A, true);
      return Promise.resolve();
    };
    await rejects(
      first.service.beginTransferScopes(stalePlan),
      "switching away and back while reserving a copy cancels before native insertion",
    );
    first.storage.beforeWrite = null;
    assert(
      !parseWorkspaceBookmarkState(first.storage.value).pendingCopy,
      "a rejected pre-copy reservation removes its marker",
    );
    assert(
      first.service.getContext(first.a).ready,
      "cancelling before native copying releases the temporary hold",
    );
    const plan = await first.service.captureTransferScopes(
      ["alpha_______"],
      "unfiled_____",
      true,
      first.service.getContext(first.a),
    );
    await first.service.beginTransferScopes(plan);
    const copy = clone(first.places.find("alpha_______")!.node);
    copy.guid = "failedCopy__";
    copy.children![0].guid = "failedBm____";
    copy.children![1].guid = "failedNest__";
    copy.children![1].children![0].guid = "failedChild_";
    first.places.find("unfiled_____")!.node.children!.push(copy);
    first.storage.failWrites = true;
    await rejects(
      first.service.applyTransferScopes(plan, { alpha_______: copy.guid }),
      "failed scope persistence is reported after native copy",
    );
    assert(
      parseWorkspaceBookmarkState(first.storage.value).pendingCopy,
      "the durable copy hold survives a failed metadata write",
    );
    first.service.destroy();
    first.storage.failWrites = false;
    const restarted = await fixture({
      storage: first.storage,
      places: first.places,
    });
    try {
      assertEquals(
        restarted.service.getStatus().state,
        "error",
        "a new service detects the interrupted copy from disk",
      );
      assert(
        !restarted.service.isVisible("failedBm____", restarted.a),
        "restarting cannot publish an unassigned copy",
      );
      const recovery = await restarted.service.readRecoverySettings();
      assert(recovery, "the pre-copy last-good settings remain available");
      await restarted.service.applyImportSettings(
        await restarted.service.previewImportSettings(recovery),
      );
      assertEquals(
        restarted.service.getStatus().state,
        "ready",
        "explicit last-good recovery releases the interrupted-copy hold",
      );
      equalJSON(
        restarted.service.getScope(copy.guid),
        [],
        "folders created during the interrupted operation are quarantined without guessing a source",
      );
      assert(
        !restarted.service.isVisible("failedBm____", restarted.a),
        "recovering old settings cannot expose the incomplete copy as common",
      );
      assert(
        restarted.service.isVisible("bookmarkA___", restarted.a),
        "recovery retains the original folder's scope",
      );
    } finally {
      restarted.service.destroy();
    }
  } finally {
    first.service.destroy();
  }
}

async function testCleanupPreservesConcurrentBookmarks(): Promise<void> {
  const { service, places, a, b } = await fixture();
  try {
    const created = await service.getOrCreateWorkspaceFolder(a, "Shared race");
    places.beforeRemove = (guid) => {
      places.find(guid)!.node.children!.push({
        guid: "racedItem___",
        typeCode: 1,
        uri: "https://example.com/raced-save",
      });
      places.beforeRemove = null;
    };
    await service.releaseCreatedWorkspaceFolder(created);
    assert(
      places.find("racedItem___"),
      "cleanup cannot delete a child saved after the empty check",
    );
    equalJSON(
      service.getScope(created.guid),
      [A],
      "a retained folder keeps its restrictive scope",
    );
    await service.refresh();
    assert(
      !service.isVisible("racedItem___", b),
      "failed cleanup never turns the retained bookmark into common data",
    );
  } finally {
    service.destroy();
  }
  const rollback = await fixture();
  try {
    rollback.places.beforeInsert = () => {
      rollback.places.nextTree = () =>
        Promise.reject(new Error("Simulated post-insert index failure"));
    };
    rollback.places.beforeRemove = (guid) => {
      rollback.places.find(guid)!.node.children!.push({
        guid: "keptItem____",
        typeCode: 1,
        uri: "https://example.com/kept-save",
      });
      rollback.places.beforeRemove = null;
    };
    await rejects(
      rollback.service.getOrCreateWorkspaceFolder(rollback.a, "Rollback race"),
      "a post-insert index failure is reported",
    );
    const retained =
      parseWorkspaceBookmarkState(rollback.storage.value).workspaceFolders[A];
    assert(
      retained && rollback.places.find("keptItem____"),
      "rollback preserves the folder and concurrently saved child",
    );
    equalJSON(
      rollback.service.getScope(retained),
      [A],
      "a failed native cleanup cannot erase the surviving folder's rule",
    );
    await rollback.service.refresh();
    assert(
      !rollback.service.isVisible("keptItem____", rollback.b),
      "the retained folder remains scoped after the index recovers",
    );
  } finally {
    rollback.service.destroy();
  }
}

async function testTenThousandBookmarkCache(): Promise<void> {
  const { service, places, a } = await fixture();
  try {
    const alpha = places.find("alpha_______")!.node;
    for (let index = 0; index < 10_000; index++) {
      alpha.children!.push({
        guid: `bm${String(index).padStart(10, "0")}`,
        typeCode: 1,
        uri: `https://example.com/bench/${index}`,
      });
    }
    await service.refresh();
    await service.setFolderScope(alpha.guid, [A]);
    const context = service.getContext(a);
    const samples: number[] = [];
    for (let iteration = 0; iteration < 30; iteration++) {
      const started = performance.now();
      const guids = service.getVisibleBookmarkGuids(context);
      const json = service.getVisibleBookmarkGuidsJSON(context);
      assert(
        guids && guids.length >= 10_000 && json && json.length > 10_000,
        "bulk GUID/SQL JSON cache retains the whole eligible set",
      );
      for (let probe = 0; probe < 100; probe++) {
        assert(
          service.isVisible(`bm${String(probe).padStart(10, "0")}`, context),
          "cached predicates remain accurate",
        );
      }
      samples.push(performance.now() - started);
    }
    samples.sort((left, right) => left - right);
    console.info(
      `[WorkspaceBookmarks benchmark] 10,000 bookmarks, 100 visibility predicates + SQL JSON p95=${
        samples[Math.floor(samples.length * 0.95)].toFixed(2)
      }ms; cold=${samples.at(-1)!.toFixed(2)}ms`,
    );
    assertEquals(
      service.getVisibleBookmarkGuids(context),
      service.getVisibleBookmarkGuids(context),
      "bulk GUID results reuse the same generation cache",
    );
  } finally {
    service.destroy();
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "GUID visibility, common contexts and ancestor intersections",
      fn: testIdentityVisibilityAndIntersections,
    },
    {
      name: "window snapshots and atomic write failures",
      fn: testWindowSnapshotsAndAtomicFailure,
    },
    {
      name: "corruption recovery and explicit import mappings",
      fn: testCorruptionRecoveryAndImport,
    },
    {
      name: "archive ID restore and dedicated save destinations",
      fn: testWorkspaceRestoreAndSaveDestinations,
    },
    {
      name: "copy subtree rules and expansion confirmations",
      fn: testTransfersPreserveSubtreeRules,
    },
    {
      name: "Places GUID remapping and concurrent refresh",
      fn: testObserverGuidRemappingAndRefreshRace,
    },
    {
      name: "interrupted copy holds survive restart and explicit recovery",
      fn: testCopyFailureSurvivesRestart,
    },
    {
      name: "cleanup preserves concurrent bookmarks and scope rules",
      fn: testCleanupPreservesConcurrentBookmarks,
    },
    {
      name: "10,000-bookmark generation cache benchmark",
      fn: testTenThousandBookmarkCache,
    },
  ];
  await runTests("WorkspaceBookmarks.test.mts", tests);
}
