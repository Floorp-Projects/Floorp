// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import Workspaces from "../index.ts";
import type { WorkspacesService } from "../workspacesService.ts";
import { workspacesDataStore } from "../data/data.ts";
import type { TFormResult } from "../../modal-parent/utils/type.ts";
import { WORKSPACE_DATA_PREF_NAME } from "../utils/workspaces-static-names.ts";
import { getWorkspaceSessionIdentityService } from "../utils/workspace-sessions.ts";
import { waitFor } from "./workspace-last-tab-test-utils.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

const ContextualIdentityService = getWorkspaceSessionIdentityService();

async function withSessionCreation(
  check: (service: WorkspacesService) => Promise<void>,
): Promise<void> {
  await waitFor(
    () => Boolean(Workspaces.getCtx()),
    "workspace service is ready",
  );
  const service = Workspaces.getCtx();
  assert(service, "workspace service exists");
  const originalWorkspace = service.getSelectedWorkspaceID();
  const originalTab = globalThis.gBrowser.selectedTab;
  const originalTabs = new Set(globalThis.gBrowser.tabs);
  const originalWorkspaces = new Set(workspacesDataStore.order);
  const originalData = Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME);
  const originalIdentities = new Set(
    ContextualIdentityService.getPublicIdentities().map((identity) =>
      identity.userContextId
    ),
  );
  const createModal = service.modalCtx.showCreateWorkspacesModal;
  const editModal = service.modalCtx.showWorkspacesModal;

  try {
    await check(service);
  } finally {
    service.modalCtx.showCreateWorkspacesModal = createModal;
    service.modalCtx.showWorkspacesModal = editModal;
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
    for (const identity of ContextualIdentityService.getPublicIdentities()) {
      if (
        !originalIdentities.has(identity.userContextId) &&
        identity.name.startsWith("Workspace session test")
      ) {
        ContextualIdentityService.remove(identity.userContextId);
      }
    }
  }
}

async function testPendingAndCancelledCreation(): Promise<void> {
  await withSessionCreation(async (service) => {
    const workspace = service.getSelectedWorkspaceID();
    const tab = globalThis.gBrowser.selectedTab;
    const data = Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME);
    const tabs = globalThis.gBrowser.tabs.length;
    const identities = ContextualIdentityService.getPublicIdentities().length;
    let complete: (result: TFormResult | null) => void = () => {
      throw new Error("creation modal did not open");
    };
    service.modalCtx.showCreateWorkspacesModal = () =>
      new Promise((resolve) => {
        complete = resolve;
      });

    const pending = service.createWorkspaceFromDialog();
    assertEquals(
      Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME),
      data,
      "opening creation leaves persisted workspaces untouched",
    );
    assertEquals(globalThis.gBrowser.tabs.length, tabs, "no premature tab");
    assertEquals(
      ContextualIdentityService.getPublicIdentities().length,
      identities,
      "no premature container",
    );
    complete(null);
    assertEquals(await pending, null, "cancel returns no workspace");
    assertEquals(
      service.getSelectedWorkspaceID(),
      workspace,
      "selection stays",
    );
    assertEquals(globalThis.gBrowser.selectedTab, tab, "selected tab stays");
    assertEquals(
      Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME),
      data,
      "cancel does not persist a workspace",
    );
  });
}

async function testRealContainerCreationSharingAndEditing(): Promise<void> {
  await withSessionCreation(async (service) => {
    service.modalCtx.showCreateWorkspacesModal = () =>
      Promise.resolve({
        name: "Workspace session test A",
        userContextId: "new",
      });
    const firstId = await service.createWorkspaceFromDialog();
    assert(firstId, "creation returns a workspace");
    const first = service.getRawWorkspace(firstId);
    assert(first && first.userContextId > 0, "new workspace has a container");
    const contextId = first.userContextId;
    const firstTab = globalThis.gBrowser.selectedTab;
    const firstBrowser = firstTab.linkedBrowser;
    assert(firstBrowser, "the first tab has a browser");
    assertEquals(
      Number(firstBrowser.getAttribute("usercontextid")),
      contextId,
      "the first browser, not just the tab label, uses the chosen container",
    );

    const documentURI = Services.io.newURI(
      "data:text/html,<title>Workspace session test</title>",
    );
    firstBrowser.loadURI(
      documentURI,
      {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      },
    );
    await waitFor(
      () =>
        firstBrowser.currentURI?.spec === documentURI.spec &&
        (firstBrowser.contentPrincipal as nsIPrincipal | null)?.originAttributes
            .userContextId === contextId,
      "the first document principal uses the container",
    );

    // Selecting an explicit standard tab must not change the sharing default.
    const exception = service.withExplicitTabUserContext(
      0,
      () =>
        globalThis.gBrowser.addTab("about:blank", {
          userContextId: 0,
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
        }),
    );
    globalThis.gBrowser.selectedTab = exception;
    assert(exception.linkedBrowser, "exception has a browser");
    assertEquals(
      Number(exception.linkedBrowser.getAttribute("usercontextid")),
      0,
      "exception is standard",
    );
    const identities = ContextualIdentityService.getPublicIdentities().length;
    let sharedContext: number | undefined;
    service.modalCtx.showCreateWorkspacesModal = (seed) => {
      sharedContext = seed.userContextId;
      return Promise.resolve({
        name: "Workspace session test B",
        userContextId: String(seed.userContextId),
      });
    };
    const secondId = await service.createWorkspaceFromDialog();
    assert(secondId, "shared workspace is created");
    assertEquals(
      sharedContext,
      contextId,
      "sharing uses workspace default even when selected tab is an exception",
    );
    assertEquals(
      service.getRawWorkspace(secondId)?.userContextId,
      contextId,
      "workspaces share the same container ID",
    );
    const secondTab = globalThis.gBrowser.selectedTab;
    const secondBrowser = secondTab.linkedBrowser;
    assert(secondBrowser, "shared workspace's first tab has a browser");
    assertEquals(
      Number(secondBrowser.getAttribute("usercontextid")),
      contextId,
      "shared workspace's first browser uses the shared container",
    );
    assertEquals(
      ContextualIdentityService.getPublicIdentities().length,
      identities,
      "sharing does not create an extra container",
    );

    service.modalCtx.showWorkspacesModal = () =>
      Promise.resolve({
        name: "Workspace session test B",
        userContextId: "0",
      });
    await service.manageWorkspaceFromDialog(secondId);
    assertEquals(
      service.getRawWorkspace(secondId)?.userContextId,
      0,
      "edit applies",
    );
    assertEquals(
      globalThis.gBrowser.selectedTab,
      secondTab,
      "edit keeps the tab",
    );
    assertEquals(
      Number(secondBrowser.getAttribute("usercontextid")),
      contextId,
      "changing the default does not recreate an existing browser",
    );
    assertEquals(
      service.getRawWorkspace(firstId)?.userContextId,
      contextId,
      "changing a workspace default does not change its sharing peer",
    );

    // An explicit standard choice must also work when leaving a container WS.
    service.changeWorkspace(firstId);
    service.modalCtx.showCreateWorkspacesModal = () =>
      Promise.resolve({
        name: "Workspace session test standard",
        userContextId: "0",
      });
    const standardId = await service.createWorkspaceFromDialog();
    assert(standardId, "explicit standard workspace is created");
    assertEquals(
      service.getRawWorkspace(standardId)?.userContextId,
      0,
      "standard is persisted",
    );
    const standardBrowser = globalThis.gBrowser.selectedTab.linkedBrowser;
    assert(standardBrowser, "standard workspace has its first browser");
    assertEquals(
      Number(standardBrowser.getAttribute("usercontextid")),
      0,
      "the previous workspace's container does not replace explicit standard",
    );
  });
}

async function testFailureAfterNativeTabCreation(): Promise<void> {
  await withSessionCreation(async (service) => {
    const previousWorkspace = service.getSelectedWorkspaceID();
    const previousTab = globalThis.gBrowser.selectedTab;
    const previousTabs = globalThis.gBrowser.tabs.length;
    const previousOrder = workspacesDataStore.order.join(",");
    const previousIdentities = ContextualIdentityService.getPublicIdentities()
      .length;
    const createTab = service.tabManagerCtx.createTabForWorkspace;
    const reportError: unknown = Reflect.get(
      service,
      "reportWorkspaceSessionError",
    );
    const errors: unknown[] = [];
    service.modalCtx.showCreateWorkspacesModal = () =>
      Promise.resolve({
        name: "Workspace session test failure",
        userContextId: "new",
      });
    service.tabManagerCtx.createTabForWorkspace = (...args) => {
      createTab.apply(service.tabManagerCtx, args);
      throw new Error("Injected failure after the native TabOpen event");
    };
    Reflect.set(service, "reportWorkspaceSessionError", (error: unknown) => {
      errors.push(error);
    });
    try {
      assertEquals(
        await service.createWorkspaceFromDialog(),
        null,
        "creation fails",
      );
      assertEquals(errors.length, 1, "failure is reported once");
      assertEquals(
        service.getSelectedWorkspaceID(),
        previousWorkspace,
        "workspace is restored",
      );
      assertEquals(
        globalThis.gBrowser.selectedTab,
        previousTab,
        "tab is restored",
      );
      assertEquals(
        workspacesDataStore.order.join(","),
        previousOrder,
        "failed workspace is removed",
      );
      await waitFor(
        () => globalThis.gBrowser.tabs.length === previousTabs,
        "the partially created native tab is removed without a replacement",
      );
      assertEquals(
        ContextualIdentityService.getPublicIdentities().length,
        previousIdentities,
        "the operation's unused container is removed",
      );
    } finally {
      service.tabManagerCtx.createTabForWorkspace = createTab;
      Reflect.set(service, "reportWorkspaceSessionError", reportError);
    }
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("workspaceSessionCreation.test.ts", [
    {
      name: "pending creation and cancellation",
      fn: testPendingAndCancelledCreation,
    },
    {
      name: "real browser context, sharing and editing",
      fn: testRealContainerCreationSharingAndEditing,
    },
    {
      name: "rollback after native tab creation",
      fn: testFailureAfterNativeTabCreation,
    },
  ]);
}
