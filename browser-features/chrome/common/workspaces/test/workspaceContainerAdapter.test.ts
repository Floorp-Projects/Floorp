// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { WorkspacesSyncController } from "../workspacesSync.ts";
import { getWorkspaceContainerBindings } from "../workspace-container-ui.ts";
import { setWorkspacesDataStore, workspacesDataStore } from "../data/data.ts";
import type {
  TWorkspace,
  TWorkspaceID,
  TWorkspacesStoreData,
} from "../utils/type.ts";
import type { WorkspaceSyncDocument } from "#modules/common/workspace-sync-types.ts";
import type {
  SyncSessionStore,
  WorkspaceSyncAPI,
} from "../workspacesSyncTypes.ts";
import type { WorkspacesService } from "../workspacesService.ts";
import { WORKSPACE_TAB_ATTRIBUTION_ID } from "../utils/workspaces-static-names.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

async function testPendingContainersNeverOpenOrOverwrite(): Promise<void> {
  const id = Services.uuid.generateUUID().toString().slice(
    1,
    -1,
  ) as TWorkspaceID;
  const tabId = Services.uuid.generateUUID().toString().slice(1, -1);
  const containerId = Services.uuid.generateUUID().toString().slice(1, -1);
  const reference = { kind: "container" as const, containerId };
  const desired: WorkspaceSyncDocument = {
    version: 2,
    workspaceId: id,
    name: "Container adapter fixture",
    icon: null,
    defaultContainer: reference,
    metadata: { modifiedAt: 1, modifiedBy: id },
    tabs: [{
      id: tabId,
      url: "https://example.invalid/container-pending",
      title: "Remote account",
      pinned: false,
      container: reference,
      position: 0,
      deleted: false,
      modifiedAt: 1,
      modifiedBy: id,
    }],
  };
  const bindings = getWorkspaceContainerBindings();
  const originalResolve = bindings.resolve;
  const originalLookup = bindings.lookup;
  const originalReady = bindings.isReady;
  const originalCapture = bindings.captureReference;
  const originalData = new Map(
    workspacesDataStore.data as unknown as Map<TWorkspaceID, TWorkspace>,
  );
  const originalOrder = [...workspacesDataStore.order];
  const { SessionStore: sessionStore } = ChromeUtils.importESModule(
    "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
  ) as { SessionStore: SyncSessionStore };
  let localTab: XULElement | undefined;
  let ready = false;
  const resolution = () =>
    ready
      ? {
        status: "resolved" as const,
        userContextId: 9876,
        instanceId: "new-instance",
      }
      : {
        status: "pending" as const,
        reason: "confirmation" as const,
        containerId,
        candidates: [],
      };
  try {
    bindings.resolve = () => Promise.resolve(resolution());
    bindings.lookup = resolution;
    bindings.isReady = () => true;
    bindings.captureReference = () => ({ ref: reference });
    const api = {
      isContainerSyncEnabled: () => true,
      isContainerDefaultPending: () => !ready,
      isContainerApplyPending: () => true,
      isSelected: () => true,
      isRetired: () => false,
      isPendingClose: () => false,
      getSyncedWorkspace: () => desired,
      getSyncedTab: (_workspaceId: string, candidate: string) =>
        candidate === tabId ? desired.tabs[0] : null,
    } as unknown as WorkspaceSyncAPI;
    const ctx = {
      getRawWorkspace: (workspaceId: TWorkspaceID) =>
        (workspacesDataStore.data as unknown as Map<TWorkspaceID, TWorkspace>)
          .get(workspaceId),
      tabManagerCtx: { updateTabsVisibility: () => {} },
    } as unknown as WorkspacesService;
    // Exercise the production adapter with the native gBrowser and SessionStore;
    // replace only the account/binding boundary and omit constructor registration.
    const controller = Object.assign(
      Object.create(WorkspacesSyncController.prototype),
      {
        window,
        api,
        ctx,
        sessionStore,
        stopped: false,
        active: false,
        applying: false,
        revision: [() => 0, () => 0],
        physicalOrder: new Map(),
        containerPendingTabs: new Set(),
        containerPendingDefaults: new Set(),
        replacementTabs: new WeakSet(),
        programmaticClosures: new WeakSet(),
        explicitlyOpened: new WeakSet(),
        incomingNavigations: new Map(),
      },
    ) as WorkspacesSyncController;
    const count = gBrowser.tabs.length;
    const pending = await controller.apply(desired, new Set([tabId]));
    assert(pending, "unresolved apply returns durable UI pending state");
    assertEquals(
      pending.pendingDefaultContainer,
      true,
      "empty/default container must remain pending",
    );
    assertEquals(
      pending.pendingContainerTabIds.join(),
      tabId,
      "missing remote tab must be retained as pending",
    );
    assertEquals(
      gBrowser.tabs.length,
      count,
      "no browsing context or request is created before resolution",
    );

    localTab = gBrowser.addTab("about:blank", {
      inBackground: true,
      userContextId: 0,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    localTab.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, id);
    sessionStore.setCustomTabValue(localTab, "floorp-workspace-sync-id", tabId);
    sessionStore.setCustomTabValue(localTab, "floorp-workspace-sync-owner", id);
    ready = true;
    const mismatch = await controller.apply(desired, new Set());
    assertEquals(
      mismatch?.pendingContainerTabIds.join(),
      tabId,
      "existing wrong-container tab requires explicit reopen",
    );
    assertEquals(
      gBrowser.getBrowserForTab(localTab).currentURI?.spec,
      "about:blank",
      "remote URL is not loaded under the old account",
    );
    const snapshot = controller.capture(id, new Set());
    assert(snapshot, "workspace can still be captured while its tab waits");
    assertEquals(
      snapshot.tabs[0]?.url,
      desired.tabs[0].url,
      "old page does not replace the pending remote URL",
    );
    assertEquals(
      JSON.stringify(snapshot.tabs[0]?.container),
      JSON.stringify(reference),
      "capture retains the desired logical container",
    );
    assertEquals(
      snapshot.protectedTabIds?.join(),
      tabId,
      "pending physical tab is protected from inferred deletion",
    );
  } finally {
    bindings.resolve = originalResolve;
    bindings.lookup = originalLookup;
    bindings.isReady = originalReady;
    bindings.captureReference = originalCapture;
    if (localTab) gBrowser.removeTab(localTab, { animate: false });
    setWorkspacesDataStore(
      "data",
      originalData as unknown as TWorkspacesStoreData["data"],
    );
    setWorkspacesDataStore("order", originalOrder);
  }
}

async function testRestoredReplacementSourceKeepsItsNewIdentity(): Promise<
  void
> {
  const uuid = () => Services.uuid.generateUUID().toString().slice(1, -1);
  const workspaceId = uuid();
  const previousId = uuid();
  const restoredId = uuid();
  const transaction = { token: uuid(), workspaceId, tabId: previousId };
  const { SessionStore: sessionStore } = ChromeUtils.importESModule(
    "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
  ) as { SessionStore: SyncSessionStore };
  const source = gBrowser.addTab("about:blank", {
    inBackground: true,
    userContextId: 0,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  try {
    sessionStore.setCustomTabValue(
      source,
      "floorp-workspace-sync-id",
      restoredId,
    );
    sessionStore.setCustomTabValue(
      source,
      "floorp-workspace-sync-reopen-source",
      JSON.stringify(transaction),
    );
    const finished: boolean[] = [];
    const controller = Object.assign(
      Object.create(WorkspacesSyncController.prototype),
      {
        sessionStore,
        api: {
          isRetired: () => false,
          finishContainerReopen: (_token: string, success: boolean) => {
            finished.push(success);
            return Promise.resolve();
          },
        },
      },
    ) as { recoverContainerReopens(): Promise<void> };
    await controller.recoverContainerReopens();
    assertEquals(
      sessionStore.getCustomTabValue(source, "floorp-workspace-sync-id"),
      restoredId,
      "Undo Close's newer stable ID is not replaced by a stale transaction ID",
    );
    assertEquals(
      sessionStore.getCustomTabValue(
        source,
        "floorp-workspace-sync-reopen-source",
      ),
      "",
      "the stale source transaction is consumed once",
    );
    assertEquals(
      finished.join(),
      "false",
      "recovery rolls back the old operation",
    );

    sessionStore.setCustomTabValue(source, "floorp-workspace-sync-id", "");
    sessionStore.setCustomTabValue(
      source,
      "floorp-workspace-sync-reopen-source",
      JSON.stringify(transaction),
    );
    await controller.recoverContainerReopens();
    assertEquals(
      sessionStore.getCustomTabValue(source, "floorp-workspace-sync-id"),
      previousId,
      "a genuinely interrupted transfer restores its unowned original ID",
    );
  } finally {
    sessionStore.setCustomTabValue(
      source,
      "floorp-workspace-sync-reopen-source",
      "",
    );
    gBrowser.removeTab(source, { animate: false });
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("workspaceContainerAdapter.test.ts", [
    {
      name:
        "private windows do not wait for an intentionally inactive sync provider",
      fn: () => {
        const controller = Object.assign(
          Object.create(WorkspacesSyncController.prototype),
          { privateWindow: true, active: false, revision: [() => 0, () => 0] },
        ) as WorkspacesSyncController;
        assertEquals(
          controller.defaultContainerPending("private" as TWorkspaceID),
          false,
          "private-window new tabs remain available without sync registration",
        );
      },
    },
    {
      name:
        "restored replacement sources preserve new IDs and recover interrupted transfers",
      fn: testRestoredReplacementSourceKeepsItsNewIdentity,
    },
    {
      name:
        "unresolved and mismatched container refs never open under another account or overwrite desired state",
      fn: testPendingContainersNeverOpenOrOverwrite,
    },
  ]);
}
