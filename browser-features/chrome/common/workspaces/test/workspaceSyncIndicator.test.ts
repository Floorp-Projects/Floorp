// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { render } from "@nora/solid-xul";
import { createEffect, createRoot, createSignal } from "solid-js";
import i18next from "i18next";
import { setLanguage } from "#i18n/config-browser-chrome.ts";
import {
  applyWorkspaceSyncIndicator,
  createWorkspaceSyncIndicator,
} from "../toolbar/workspace-sync-indicator.ts";
import { PopupToolbarElement } from "../toolbar/popup-block-element.tsx";
import { WorkspacePanelButton } from "../toolbar/workspaces-panels.tsx";
import { WorkspaceIcons } from "../utils/workspace-icons.ts";
import { setWorkspacesDataStore, workspacesDataStore } from "../data/data.ts";
import { WORKSPACE_DATA_PREF_NAME } from "../utils/workspaces-static-names.ts";
import type { WorkspacesService } from "../workspacesService.ts";
import type {
  TWorkspace,
  TWorkspaceID,
  TWorkspacesStoreData,
} from "../utils/type.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

const ERROR = "Fixture sync issue";

function createBoundary() {
  const firstId = Services.uuid.generateUUID().toString().slice(
    1,
    -1,
  ) as TWorkspaceID;
  const secondId = Services.uuid.generateUUID().toString().slice(
    1,
    -1,
  ) as TWorkspaceID;
  const workspaces = new Map<TWorkspaceID, TWorkspace>([
    [firstId, {
      name: "Alpha",
      icon: "floorp-icon:v1:book",
      userContextId: 1,
      isDefault: null,
      isSelected: null,
    }],
    [secondId, {
      name: "Beta",
      icon: "floorp-icon:v1:music",
      userContextId: 0,
      isDefault: null,
      isSelected: null,
    }],
  ]);
  const [selected, setSelected] = createSignal<ReadonlySet<TWorkspaceID>>(
    new Set(),
  );
  const [errors, setErrors] = createSignal<ReadonlyMap<TWorkspaceID, string>>(
    new Map(),
  );
  const ctx = {
    getRawWorkspace: (id: TWorkspaceID) => workspaces.get(id),
    iconCtx: new WorkspaceIcons(),
    syncController: {
      isAvailable: () => true,
      isSelected: (id: TWorkspaceID) => selected().has(id),
      errorText: (id: TWorkspaceID) => errors().get(id) ?? null,
    },
  } as unknown as WorkspacesService;
  return { ctx, firstId, secondId, workspaces, setSelected, setErrors };
}

async function settle(): Promise<void> {
  await Promise.resolve();
  await Promise.resolve();
}

async function withEnglish(test: () => Promise<void>): Promise<void> {
  assert(
    i18next.isInitialized,
    "browser chrome translations must be initialized",
  );
  const originalLocale = i18next.language;
  try {
    await i18next.changeLanguage("en-US");
    setLanguage("en-US");
    await settle();
    await test();
  } finally {
    await i18next.changeLanguage(originalLocale);
    setLanguage(originalLocale);
  }
}

function assertIndicator(
  node: Element,
  state: "selected" | "attention" | undefined,
  tooltip: string,
  tooltipAttribute = "tooltiptext",
): void {
  assertEquals(
    node.getAttribute("data-workspace-sync"),
    state ?? null,
    "the rendered marker must track the workspace's sync selection and error",
  );
  assertEquals(node.getAttribute(tooltipAttribute), tooltip, "visible tooltip");
  assertEquals(node.getAttribute("aria-label"), tooltip, "accessible name");
}

async function testToolbarUpdatesForSameWorkspaceAndPreservesContent(): Promise<
  void
> {
  await withEnglish(async () => {
    const boundary = createBoundary();
    const node = document.createXULElement("toolbarbutton");
    node.setAttribute("label", "Alpha");
    node.setAttribute(
      "style",
      "list-style-image: url(chrome://branding/content/icon32.png)",
    );
    node.setAttribute("data-has-container", "true");
    node.setAttribute("data-container-color", "purple");
    document.documentElement.appendChild(node);
    const originalIcon = node.getAttribute("style");
    let dispose = () => {};
    try {
      createRoot((cleanup) => {
        dispose = cleanup;
        const indicator = createWorkspaceSyncIndicator(
          boundary.ctx,
          () => boundary.firstId,
          () => "Alpha",
        );
        createEffect(() => applyWorkspaceSyncIndicator(node, indicator()));
      });
      await settle();
      assertIndicator(node, undefined, "Alpha");
      boundary.setSelected(new Set([boundary.firstId]));
      await settle();
      assertIndicator(node, "selected", "Alpha — Included in workspace sync");
      boundary.setErrors(new Map([[boundary.firstId, ERROR]]));
      await settle();
      assertIndicator(
        node,
        "attention",
        `Alpha — Workspace sync needs attention: ${ERROR}`,
      );
      boundary.setErrors(new Map<TWorkspaceID, string>());
      await settle();
      assertIndicator(node, "selected", "Alpha — Included in workspace sync");
      boundary.setSelected(new Set<TWorkspaceID>());
      await settle();
      assertIndicator(node, undefined, "Alpha");
      assertEquals(
        node.getAttribute("label"),
        "Alpha",
        "status must not replace the workspace name",
      );
      assertEquals(
        node.getAttribute("style"),
        originalIcon,
        "status must not replace the workspace icon",
      );
      assertEquals(
        node.getAttribute("data-has-container"),
        "true",
        "container indicator remains present",
      );
      assertEquals(
        node.getAttribute("data-container-color"),
        "purple",
        "container color remains unchanged",
      );

      node.removeAttribute("label");
      boundary.setSelected(new Set([boundary.firstId]));
      await settle();
      assert(
        !node.hasAttribute("label"),
        "name-hidden toolbar mode must stay name-hidden",
      );
      assertIndicator(node, "selected", "Alpha — Included in workspace sync");
    } finally {
      dispose();
      node.remove();
    }
  });
}

async function testPopupAndSidebarTrackStatusAndWorkspaceSwitches(): Promise<
  void
> {
  await withEnglish(async () => {
    const boundary = createBoundary();
    const originalData = workspacesDataStore.data;
    const seededData = new Map(
      originalData as unknown as Map<TWorkspaceID, TWorkspace>,
    );
    for (const [id, workspace] of boundary.workspaces) {
      seededData.set(id, workspace);
    }
    const hadPreference = Services.prefs.prefHasUserValue(
      WORKSPACE_DATA_PREF_NAME,
    );
    const originalPreference = Services.prefs.getStringPref(
      WORKSPACE_DATA_PREF_NAME,
      "",
    );
    const popupHost = document.createXULElement("vbox");
    const sidebarHost = document.createElement("div");
    document.documentElement.append(popupHost, sidebarHost);
    const [workspaceId, setWorkspaceId] = createSignal(boundary.firstId);
    let disposeComponents = () => {};
    try {
      setWorkspacesDataStore(
        "data",
        seededData as unknown as TWorkspacesStoreData["data"],
      );
      createRoot((cleanup) => {
        disposeComponents = cleanup;
        render(() =>
          PopupToolbarElement({
            get workspaceId() {
              return workspaceId();
            },
            isSelected: true,
            bmsMode: false,
            ctx: boundary.ctx,
          }), popupHost);
        render(() =>
          WorkspacePanelButton({
            get workspaceId() {
              return workspaceId();
            },
            ctx: boundary.ctx,
          }), sidebarHost);
      });
      await settle();
      const popup = popupHost.querySelector(".workspaceButton");
      const sidebar = sidebarHost.querySelector(".workspace-panel");
      assert(
        popup && sidebar,
        "both production workspace entry components must render",
      );
      const firstPopupIcon = popup.getAttribute("style");
      const firstSidebarIcon = sidebar.querySelector("img")?.getAttribute(
        "src",
      );
      const containerColor = popup.getAttribute("data-container-color");
      const checked = sidebar.getAttribute("data-checked");
      const assertBoth = (
        state: "selected" | "attention" | undefined,
        tooltip: string,
      ) => {
        assertIndicator(popup, state, tooltip);
        assertIndicator(sidebar, state, tooltip, "title");
      };
      assertBoth(undefined, "Alpha");
      boundary.setSelected(new Set([boundary.firstId]));
      await settle();
      assertBoth("selected", "Alpha — Included in workspace sync");
      boundary.setErrors(new Map([[boundary.firstId, ERROR]]));
      await settle();
      assertBoth(
        "attention",
        `Alpha — Workspace sync needs attention: ${ERROR}`,
      );
      boundary.setSelected(new Set<TWorkspaceID>());
      await settle();
      assertBoth(undefined, "Alpha");
      boundary.setSelected(new Set([boundary.firstId]));
      await settle();
      assertBoth(
        "attention",
        `Alpha — Workspace sync needs attention: ${ERROR}`,
      );
      boundary.setErrors(new Map<TWorkspaceID, string>());
      await settle();
      assertBoth("selected", "Alpha — Included in workspace sync");
      boundary.setSelected(new Set<TWorkspaceID>());
      await settle();
      assertBoth(undefined, "Alpha");
      assertEquals(
        popup.getAttribute("label"),
        "Alpha",
        "popup name survives status changes",
      );
      assertEquals(
        popup.getAttribute("style"),
        firstPopupIcon,
        "popup icon survives status changes",
      );
      assertEquals(
        sidebar.querySelector("img")?.getAttribute("src"),
        firstSidebarIcon,
        "sidebar icon survives status changes",
      );
      assertEquals(
        popup.getAttribute("data-has-container"),
        "true",
        "popup container border survives status changes",
      );
      assertEquals(
        popup.getAttribute("data-container-color"),
        containerColor,
        "popup container color survives status changes",
      );
      assertEquals(
        popup.getAttribute("data-selected"),
        "true",
        "current workspace selection is independent of sync opt-in",
      );
      assertEquals(
        sidebar.getAttribute("data-checked"),
        checked,
        "sync opt-in must not change the active sidebar workspace",
      );

      boundary.setSelected(new Set([boundary.firstId]));
      setWorkspaceId(boundary.secondId);
      await settle();
      assertBoth(undefined, "Beta");
      assertEquals(
        popup.getAttribute("label"),
        "Beta",
        "switching workspace updates the visible name",
      );
      assertEquals(
        popup.getAttribute("data-has-container"),
        "false",
        "new workspace uses its own container state",
      );
      assertEquals(
        sidebar.querySelector("img")?.getAttribute("src"),
        boundary.ctx.iconCtx.getWorkspaceIconUrl("floorp-icon:v1:music"),
        "new workspace uses its own icon",
      );
      boundary.setSelected(new Set([boundary.firstId, boundary.secondId]));
      await settle();
      assertBoth("selected", "Beta — Included in workspace sync");
    } finally {
      disposeComponents();
      popupHost.remove();
      sidebarHost.remove();
      setWorkspacesDataStore("data", originalData);
      await settle();
      if (hadPreference) {
        Services.prefs.setStringPref(
          WORKSPACE_DATA_PREF_NAME,
          originalPreference,
        );
      } else Services.prefs.clearUserPref(WORKSPACE_DATA_PREF_NAME);
    }
  });
}

async function testIndicatorLocalizesAndTracksWorkspaceNames(): Promise<void> {
  await withEnglish(async () => {
    const boundary = createBoundary();
    boundary.setSelected(new Set([boundary.firstId]));
    const [name, setName] = createSignal("Alpha");
    const node = document.createXULElement("toolbarbutton");
    let dispose = () => {};
    try {
      createRoot((cleanup) => {
        dispose = cleanup;
        const indicator = createWorkspaceSyncIndicator(
          boundary.ctx,
          () => boundary.firstId,
          name,
        );
        createEffect(() => applyWorkspaceSyncIndicator(node, indicator()));
      });
      await settle();
      assertIndicator(node, "selected", "Alpha — Included in workspace sync");
      await i18next.changeLanguage("ja-JP");
      setLanguage("ja-JP");
      await settle();
      assertIndicator(node, "selected", "Alpha — 同期対象");
      setName("仕事 🌙");
      await settle();
      assertIndicator(node, "selected", "仕事 🌙 — 同期対象");
      boundary.setErrors(new Map([[boundary.firstId, ERROR]]));
      await settle();
      assertIndicator(
        node,
        "attention",
        `仕事 🌙 — 同期に注意が必要です: ${ERROR}`,
      );
      boundary.setSelected(new Set<TWorkspaceID>());
      await settle();
      assertIndicator(node, undefined, "仕事 🌙");
    } finally {
      dispose();
    }
  });
}

function testMissingControllerOrWorkspaceIsLocal(): void {
  createRoot((dispose) => {
    try {
      const boundary = createBoundary();
      boundary.setSelected(new Set([boundary.firstId]));
      const [nullableId, setNullableId] = createSignal<TWorkspaceID | null>(
        null,
      );
      const missingId = createWorkspaceSyncIndicator(
        boundary.ctx,
        () => nullableId() ?? undefined,
        () => nullableId() ? "Alpha" : "No workspace",
      );
      const missingController = createWorkspaceSyncIndicator(
        {} as WorkspacesService,
        () => boundary.firstId,
        () => "Local workspace",
      );
      assertEquals(
        missingId().state,
        undefined,
        "missing workspace has no sync badge",
      );
      assertEquals(
        missingId().tooltip,
        "No workspace",
        "missing workspace keeps its accessible name",
      );
      setNullableId(boundary.firstId);
      assertEquals(
        missingId().state,
        "selected",
        "initially missing selection becomes a synced workspace",
      );
      setNullableId(null);
      assertEquals(
        missingId().state,
        undefined,
        "cleared selection removes its marker",
      );
      assertEquals(
        missingId().tooltip,
        "No workspace",
        "cleared selection removes the previous workspace name",
      );
      assertEquals(
        missingController().state,
        undefined,
        "private or unavailable controller has no sync badge",
      );
      assertEquals(
        missingController().tooltip,
        "Local workspace",
        "local workspace keeps its accessible name",
      );
      const privateLike = createWorkspaceSyncIndicator(
        {
          syncController: { isSelected: () => false, errorText: () => ERROR },
        } as unknown as WorkspacesService,
        () => boundary.firstId,
        () => "Private workspace",
      );
      assertEquals(
        privateLike().state,
        undefined,
        "private or inactive selection must hide a stale error badge",
      );
      assertEquals(
        privateLike().tooltip,
        "Private workspace",
        "private or inactive workspace keeps its own name",
      );
    } finally {
      dispose();
    }
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("workspaceSyncIndicator.test.ts", [
    {
      name:
        "main toolbar updates for the same workspace without changing icon or label",
      fn: testToolbarUpdatesForSameWorkspaceAndPreservesContent,
    },
    {
      name:
        "production popup and sidebar entries follow sync state and workspace changes",
      fn: testPopupAndSidebarTrackStatusAndWorkspaceSwitches,
    },
    {
      name: "indicator localizes and tracks workspace names",
      fn: testIndicatorLocalizesAndTracksWorkspaceNames,
    },
    {
      name: "missing controller or workspace remains local",
      fn: testMissingControllerOrWorkspaceIsLocal,
    },
  ]);
}
