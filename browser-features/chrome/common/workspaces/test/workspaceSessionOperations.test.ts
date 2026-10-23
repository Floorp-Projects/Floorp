// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import Workspaces from "../index.ts";
import type { WorkspacesService } from "../workspacesService.ts";
import ModalParent from "../../modal-parent/index.ts";
import type { TFormResult } from "../../modal-parent/utils/type.ts";
import { enabled, setEnabled } from "../data/config.ts";
import { setWorkspacesDataStore, workspacesDataStore } from "../data/data.ts";
import type {
  TWorkspace,
  TWorkspaceID,
  TWorkspacesStoreData,
} from "../utils/type.ts";
import type { WorkspaceSessionTabBrowser } from "../utils/workspace-session-types.ts";
import { WORKSPACE_DATA_PREF_NAME } from "../utils/workspaces-static-names.ts";
import { getWorkspaceSessionIdentityService } from "../utils/workspace-sessions.ts";
import { getWorkspaceSessionRuntime } from "../utils/workspace-session-runtime.ts";
import { getOriginUserContextId } from "../utils/tab-replacement-lifecycle.ts";
import { waitFor, withWindows } from "./workspace-last-tab-test-utils.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const identities = getWorkspaceSessionIdentityService();
const missingId = 2_147_483_600;
const principal = () => Services.scriptSecurityManager.getSystemPrincipal();

async function withOperations(
  check: (
    service: WorkspacesService,
    create: (context: number) => Promise<TWorkspaceID>,
  ) => Promise<void>,
): Promise<void> {
  await waitFor(() => Boolean(Workspaces.getCtx()), "workspaces initialized");
  const service = Workspaces.getCtx();
  assert(service, "workspace service exists");
  const originalData = Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME);
  const originalEnabled = enabled();
  const originalWorkspace = service.getSelectedWorkspaceID();
  const originalTab = globalThis.gBrowser.selectedTab;
  const originalTabs = new Set(globalThis.gBrowser.tabs);
  const originalWorkspaces = new Set(workspacesDataStore.order);
  const modal = ModalParent.getInstance();
  const show = modal.showNoraModal;
  const createModal = service.modalCtx.showCreateWorkspacesModal;
  const context = identities.create(
    "Workspace operation test",
    "fingerprint",
    "blue",
  );
  const create = async (userContextId: number) => {
    service.modalCtx.showCreateWorkspacesModal = () =>
      Promise.resolve({
        name: "Workspace operation test",
        userContextId: String(userContextId),
      });
    // Missing IDs cannot be chosen in the creation form. Start with a valid
    // default and then simulate removal of its saved container definition.
    const id = await service.createWorkspaceFromDialog();
    assert(id, "test workspace is created");
    return id;
  };
  try {
    setEnabled(true);
    await check(service, create);
  } finally {
    modal.showNoraModal = show;
    service.modalCtx.showCreateWorkspacesModal = createModal;
    setEnabled(true);
    service.changeWorkspace(originalWorkspace);
    globalThis.gBrowser.selectedTab = originalTab;
    for (const id of [...workspacesDataStore.order]) {
      if (!originalWorkspaces.has(id)) service.deleteWorkspace(id);
    }
    for (const tab of [...globalThis.gBrowser.tabs]) {
      if (!originalTabs.has(tab)) globalThis.gBrowser.removeTab(tab);
    }
    Services.prefs.setStringPref(WORKSPACE_DATA_PREF_NAME, originalData);
    service.changeWorkspace(originalWorkspace);
    globalThis.gBrowser.selectedTab = originalTab;
    identities.remove(context.userContextId);
    setEnabled(originalEnabled);
  }
}

function setDefault(
  service: WorkspacesService,
  id: TWorkspaceID,
  context: number,
): void {
  const workspace = service.getRawWorkspace(id);
  assert(workspace, "workspace exists");
  setWorkspacesDataStore("data", (previous) => {
    const next = new Map(previous as unknown as Map<TWorkspaceID, TWorkspace>);
    next.set(id, { ...workspace, userContextId: context });
    return next as unknown as TWorkspacesStoreData["data"];
  });
}

function respond(result: TFormResult | null): void {
  ModalParent.getInstance().showNoraModal = (_form, _options, callback) => {
    callback(result);
    return Promise.resolve(result);
  };
}

const tests: TestCase[] = [
  {
    name:
      "Native tab creation resolves the context before its first browser and keeps origin and explicit standard",
    async fn() {
      await withOperations(async (service, create) => {
        const identity = identities.getPublicIdentities().find((item) =>
          item.name === "Workspace operation test"
        );
        assert(identity, "test identity exists");
        await create(identity.userContextId);
        const independent = globalThis.gBrowser.addTab("about:blank", {
          triggeringPrincipal: principal(),
        });
        assertEquals(
          Number(independent.linkedBrowser?.getAttribute("usercontextid")),
          identity.userContextId,
          "independent first browser uses workspace default",
        );
        const standard = globalThis.gBrowser.addTab("about:blank", {
          userContextId: 0,
          triggeringPrincipal: principal(),
        });
        assertEquals(
          Number(standard.linkedBrowser?.getAttribute("usercontextid")),
          0,
          "explicit standard remains standard",
        );
        globalThis.gBrowser.selectedTab = standard;
        const derived =
          (globalThis.gBrowser as unknown as WorkspaceSessionTabBrowser).addTab(
            "about:blank",
            {
              relatedToCurrent: true,
              triggeringPrincipal: principal(),
            },
          );
        assertEquals(
          Number(derived.linkedBrowser?.getAttribute("usercontextid")),
          0,
          "origin standard takes priority over workspace default",
        );
        const restored = globalThis.gBrowser.addTab("about:blank", {
          userContextId: identity.userContextId,
          triggeringPrincipal: principal(),
        });
        globalThis.gBrowser.selectedTab = restored;
        const { SessionStore } = ChromeUtils.importESModule(
          "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
        );
        const restoredURL = Services.io.newURI(
          "data:text/html,<title>Restored workspace session</title>",
        ).spec;
        SessionStore.setTabState(
          restored,
          JSON.stringify({
            entries: [{ url: restoredURL }],
            index: 1,
            userContextId: identity.userContextId,
          }),
        );
        await waitFor(
          () =>
            restored.linkedBrowser?.currentURI?.spec === restoredURL &&
            (restored.linkedBrowser?.contentPrincipal as nsIPrincipal | null)
                ?.originAttributes.userContextId === identity.userContextId,
          "ordinary SessionStore restore loads its saved page in its saved origin context",
        );
        assertEquals(
          getWorkspaceSessionRuntime().getPendingState(restored),
          null,
          "valid restore is not held",
        );
        setEnabled(false);
        const disabled = globalThis.gBrowser.addTab("about:blank", {
          triggeringPrincipal: principal(),
        });
        assertEquals(
          Number(disabled.linkedBrowser?.getAttribute("usercontextid")),
          0,
          "disabled workspaces do not apply their default",
        );
        assert(
          !disabled.hasAttribute("floorpWorkspaceId"),
          "tabs created while disabled remain unassigned",
        );
        assertEquals(
          service.getCurrentWorkspaceUserContextId(),
          0,
          "routing API follows disable",
        );
        service.tabManagerCtx.updateTabsVisibility();
        assert(
          [...globalThis.gBrowser.tabs].every((tab) => !tab.hidden),
          "disabling shows every workspace tab",
        );
      });
    },
  },
  {
    name:
      "Missing defaults and restore states retain URLs without loading them in standard",
    async fn() {
      await withOperations(async (service, create) => {
        const id = await create(0);
        setDefault(service, id, missingId);
        const runtime = getWorkspaceSessionRuntime();
        const tab = globalThis.gBrowser.addTab(
          "https://example.invalid/workspace-held",
          { triggeringPrincipal: principal() },
        );
        assertEquals(
          tab.linkedBrowser?.currentURI?.spec,
          "about:blank",
          "missing default never starts requested page",
        );
        assertEquals(
          getOriginUserContextId(tab.linkedBrowser),
          0,
          "only the held blank browser uses the standard origin context",
        );
        assertEquals(
          runtime.getPendingState(tab)?.userContextId,
          missingId,
          "pending state retains missing identity",
        );
        const saved = {
          userContextId: missingId,
          floorpWorkspaceId: id,
          entries: [{ url: "https://example.invalid/restored" }],
          index: 1,
          formdata: { id: { secret: "do not transfer" } },
          storage: { secret: "old session" },
        };
        const safe = runtime.sanitizeRestoreState(saved);
        assertEquals(
          safe.entries?.[0]?.url,
          "about:blank",
          "restore placeholder contains no source URL",
        );
        assertEquals(
          safe.userContextId,
          0,
          "only blank placeholder uses standard",
        );
        assert(
          !("formdata" in safe) && !("storage" in safe),
          "old site state is not injected into a new browser",
        );
        assertEquals(
          JSON.parse(safe.extData?.["floorp.workspace.pendingSession"] ?? "{}")
            .formdata.id.secret,
          "do not transfer",
          "original restore data remains recoverable",
        );
        const again = runtime.sanitizeRestoreState(safe);
        assertEquals(
          again.extData?.["floorp.workspace.pendingSession"],
          safe.extData?.["floorp.workspace.pendingSession"],
          "repeated restore retains the original state",
        );
        const { SessionStore } = ChromeUtils.importESModule(
          "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
        );
        const persisted = JSON.parse(SessionStore.getTabState(tab));
        assertEquals(
          JSON.parse(persisted.extData["floorp.workspace.pendingSession"])
            .entries[0].url,
          "https://example.invalid/workspace-held",
          "pending request survives a SessionStore save",
        );
        const keepAlive = Reflect.apply(
          Reflect.get(
            service.tabManagerCtx,
            "reuseOrReplaceTrackedReplacement",
          ),
          service.tabManagerCtx,
          [null, id, false],
        );
        assertEquals(
          runtime.getPendingState(keepAlive)?.userContextId,
          missingId,
          "last-tab replacement accepts a safe held browser for a missing default",
        );
      });
    },
  },
  {
    name:
      "Archived pending state is rebound to the restored workspace before any page load",
    async fn() {
      await withOperations(async (service, create) => {
        const source = await create(0);
        const original = globalThis.gBrowser.selectedTab;
        const runtime = getWorkspaceSessionRuntime();
        runtime.markPendingTab(original, {
          userContextId: missingId,
          floorpWorkspaceId: source,
          entries: [{ url: "https://example.invalid/archived-held" }],
          index: 1,
          formdata: { secret: "original state" },
        });
        const snapshot = await service.captureWorkspaceSnapshot(source);
        assert(snapshot, "pending workspace snapshot exists");
        const destination = await create(0);
        await Reflect.apply(
          Reflect.get(service, "restoreTabsFromSnapshot"),
          service,
          [destination, snapshot.tabs],
        );
        const restored = runtime.pendingTabs(destination, missingId);
        assertEquals(
          restored.length,
          1,
          "restored placeholder belongs to the new workspace",
        );
        assertEquals(
          restored[0].state.floorpWorkspaceId,
          destination,
          "held original state also points at the new workspace",
        );
        assertEquals(
          restored[0].tab.linkedBrowser?.currentURI?.spec,
          "about:blank",
          "archived missing URL does not load in standard",
        );
        assert(
          runtime.getPendingState(original),
          "source placeholder is untouched",
        );
      });
    },
  },
  {
    name:
      "Cross-session move confirms preserve, cancel and URL reopen while keeping originals",
    async fn() {
      await withOperations(async (service, create) => {
        const source = await create(0);
        const original = globalThis.gBrowser.selectedTab;
        const identity = identities.getPublicIdentities().find((item) =>
          item.name === "Workspace operation test"
        );
        assert(identity, "test identity exists");
        const target = await create(identity.userContextId);
        service.changeWorkspace(source);
        respond(null);
        assertEquals(
          await service.tabManagerCtx.requestMoveTabsToWorkspace(target, [
            original,
          ]),
          false,
          "cancel does not move",
        );
        assertEquals(
          original.getAttribute("floorpWorkspaceId"),
          source,
          "cancel retains source workspace",
        );
        respond({ action: "preserve" });
        assertEquals(
          await service.tabManagerCtx.requestMoveTabsToWorkspace(target, [
            original,
          ]),
          true,
          "preserve moves",
        );
        assertEquals(
          original.getAttribute("floorpWorkspaceId"),
          target,
          "original moved to destination",
        );
        assertEquals(
          Number(original.linkedBrowser?.getAttribute("usercontextid")),
          0,
          "browser context remains original",
        );
        const beforeExplicit = new Set(globalThis.gBrowser.tabs);
        assertEquals(
          service.tabManagerCtx.openTabsInWorkspaceSession(target, [original]),
          true,
          "exception tab can explicitly open in its own workspace default",
        );
        const explicit = [...globalThis.gBrowser.tabs].find((tab) =>
          !beforeExplicit.has(tab)
        );
        assert(explicit, "explicit action creates a tab");
        assertEquals(
          explicit.getAttribute("floorpWorkspaceId"),
          target,
          "explicit action retains owning workspace",
        );
        assertEquals(
          Number(explicit.linkedBrowser?.getAttribute("usercontextid")),
          identity.userContextId,
          "explicit action uses owning workspace's default",
        );
        assert(
          original.isConnected,
          "explicit action keeps original input and tab",
        );
        service.tabManagerCtx.moveTabToWorkspace(source, original);
        const before = new Set(globalThis.gBrowser.tabs);
        respond({ action: "reopen" });
        assertEquals(
          await service.tabManagerCtx.requestMoveTabsToWorkspace(target, [
            original,
          ]),
          true,
          "URL reopen succeeds",
        );
        assert(original.isConnected, "original remains open");
        assertEquals(
          original.getAttribute("floorpWorkspaceId"),
          source,
          "original remains in source workspace",
        );
        const opened = [...globalThis.gBrowser.tabs].filter((tab) =>
          !before.has(tab)
        );
        assertEquals(opened.length, 1, "one destination tab is created");
        assertEquals(
          Number(opened[0].linkedBrowser?.getAttribute("usercontextid")),
          identity.userContextId,
          "new browser has destination context",
        );
      });
    },
  },
  {
    name:
      "A pending move is cancelled if source or destination changes during confirmation",
    async fn() {
      await withOperations(async (service, create) => {
        const source = await create(0);
        const tab = globalThis.gBrowser.selectedTab;
        const identity = identities.getPublicIdentities().find((item) =>
          item.name === "Workspace operation test"
        );
        assert(identity, "test identity exists");
        const target = await create(identity.userContextId);
        service.changeWorkspace(source);
        let complete: (result: TFormResult | null) => void = () => {
          throw new Error("modal was not shown");
        };
        ModalParent.getInstance().showNoraModal = (
          _form,
          _options,
          callback,
        ) => {
          complete = callback;
          return Promise.resolve(null);
        };
        const operation = service.tabManagerCtx.requestMoveTabsToWorkspace(
          target,
          [tab],
        );
        setDefault(service, target, 0);
        complete({ action: "preserve" });
        assertEquals(
          await operation,
          false,
          "stale target default cancels confirmation",
        );
        assertEquals(
          tab.getAttribute("floorpWorkspaceId"),
          source,
          "source remains intact",
        );
      });
    },
  },
  {
    name:
      "Recovery maps only one workspace and old identity across normal windows",
    async fn() {
      await withOperations(async (service, create) => {
        const id = await create(0);
        setDefault(service, id, missingId);
        const runtime = getWorkspaceSessionRuntime();
        await withWindows(false, false, async (open) => {
          const other = await open();
          const local = globalThis.gBrowser.addTab("about:blank", {
            userContextId: 0,
            triggeringPrincipal: principal(),
          });
          const remote = other.gBrowser.addTab("about:blank", {
            userContextId: 0,
            triggeringPrincipal: principal(),
          });
          const foreign = other.gBrowser.addTab("about:blank", {
            userContextId: 0,
            triggeringPrincipal: principal(),
          });
          for (const tab of [local, remote]) {
            runtime.markPendingTab(tab, {
              userContextId: missingId,
              floorpWorkspaceId: id,
              entries: [{ url: "about:blank" }],
              formdata: { secret: "old form" },
            });
          }
          runtime.markPendingTab(foreign, {
            userContextId: missingId,
            floorpWorkspaceId: "other-workspace",
            entries: [{ url: "about:blank" }],
          });
          const identity = identities.getPublicIdentities().find((item) =>
            item.name === "Workspace operation test"
          );
          assert(identity, "test identity exists");
          respond({ userContextId: String(identity.userContextId) });
          assertEquals(
            await service.recoverMissingSessionFromDialog(missingId, id),
            true,
            "recovery succeeds",
          );
          assertEquals(
            service.getRawWorkspace(id)?.userContextId,
            identity.userContextId,
            "workspace default follows mapping",
          );
          assert(
            !local.isConnected && !remote.isConnected,
            "both pending placeholders are replaced",
          );
          assert(
            foreign.isConnected && runtime.getPendingState(foreign),
            "other workspace remains pending",
          );
          assertEquals(
            runtime.pendingTabs(id, missingId).length,
            0,
            "no matching pending tabs remain",
          );
          const next = globalThis.gBrowser.addTab("about:blank", {
            triggeringPrincipal: principal(),
          });
          assertEquals(
            Number(next.linkedBrowser?.getAttribute("usercontextid")),
            identity.userContextId,
            "next independent tab uses recovered default",
          );
        });
      });
    },
  },
  {
    name:
      "Deletion and archive keep definitions and hidden tabs in another normal window",
    async fn() {
      await withOperations(async (service, create) => {
        const id = await create(0);
        const report = Reflect.get(service, "reportWorkspaceInUse");
        let reports = 0;
        Reflect.set(service, "reportWorkspaceInUse", () => {
          reports++;
        });
        try {
          await withWindows(false, false, async (open) => {
            const other = await open();
            const remote = other.gBrowser.addTab("about:blank", {
              userContextId: 0,
              triggeringPrincipal: principal(),
            });
            remote.setAttribute("floorpWorkspaceId", id);
            other.gBrowser.hideTab(remote);
            assert(
              service.isWorkspaceInUseByOtherWindow(id),
              "hidden tab counts as use",
            );
            service.deleteWorkspace(id);
            assert(
              service.isWorkspaceID(id),
              "delete preserves shared definition",
            );
            assertEquals(
              await service.archiveWorkspace(id),
              null,
              "archive is blocked",
            );
            assert(remote.isConnected, "remote tab remains open");
            assertEquals(
              reports,
              2,
              "both blocked operations explain the conflict",
            );
            const selected = other.workspacesFuncs?.getSelectedWorkspaceID();
            assert(
              selected && selected !== id,
              "other window selects a different workspace",
            );
            remote.setAttribute("floorpWorkspaceId", selected);
            const save = service.archiveService.saveSnapshot;
            const remove = service.archiveService.removeSnapshot;
            let removed = "";
            service.archiveService.saveSnapshot = () => {
              remote.setAttribute("floorpWorkspaceId", id);
              return Promise.resolve("workspace-operation-race");
            };
            service.archiveService.removeSnapshot = (archiveId) => {
              removed = archiveId;
              return Promise.resolve();
            };
            try {
              assertEquals(
                await service.archiveWorkspace(id),
                null,
                "archive rechecks after saving",
              );
              assert(service.isWorkspaceID(id), "race retains definition");
              assert(remote.isConnected, "race retains remote tab");
              assertEquals(
                removed,
                "workspace-operation-race",
                "incomplete archive is discarded",
              );
              assertEquals(reports, 3, "race is explained");
            } finally {
              service.archiveService.saveSnapshot = save;
              service.archiveService.removeSnapshot = remove;
            }
          });
        } finally {
          Reflect.set(service, "reportWorkspaceInUse", report);
        }
      });
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("workspaceSessionOperations.test.ts", tests);
}
