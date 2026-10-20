// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { render } from "@nora/solid-xul";
import { createEffect, createRoot, createSignal } from "solid-js";
import i18next from "i18next";
import { setLanguage } from "#i18n/config-browser-chrome.ts";
import {
  applyWorkspaceSyncIndicator,
  bindWorkspaceSyncFocusTooltip,
  createWorkspaceSyncIndicator,
} from "../toolbar/workspace-sync-indicator.ts";
import { PopupToolbarElement } from "../toolbar/popup-block-element.tsx";
import { WorkspacePanelButton } from "../toolbar/workspaces-panels.tsx";
import workspacesStyles from "../toolbar/styles.css?inline";
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

async function waitForUI(
  condition: () => boolean,
  message: string,
): Promise<void> {
  const deadline = performance.now() + 3000;
  while (!condition()) {
    assert(performance.now() < deadline, message);
    await new Promise<void>((resolve) => globalThis.setTimeout(resolve, 20));
  }
}

async function activateTestWindow(): Promise<void> {
  window.focus();
  await waitForUI(
    () =>
      Object.is(Services.focus.activeWindow, window) && document.hasFocus() &&
      !document.documentElement.matches(":-moz-window-inactive"),
    "the test window becomes natively active before exercising focus",
  );
}

async function withEnglish(test: () => void | Promise<void>): Promise<void> {
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

function assertPopupText(
  node: Element,
  name: string,
  status: string,
): void {
  const nameLabel = node.querySelector(".workspace-name");
  const statusLine = node.querySelector(".workspace-sync-status-line");
  const statusIcon = statusLine?.querySelector(".workspace-sync-status-icon");
  const statusLabel = node.querySelector(".workspace-sync-status");
  assert(nameLabel, "popup must retain its workspace name");
  assertEquals(
    nameLabel.getAttribute("value"),
    name,
    "workspace name is preserved",
  );
  if (!status) {
    assertEquals(statusLine, null, "excluded workspaces have no status row");
    assertEquals(
      statusLabel,
      null,
      "excluded workspaces have no status caption",
    );
    return;
  }
  assert(
    statusLine && statusIcon && statusLabel,
    "included workspaces render a status row with separate icon and text",
  );
  assertEquals(
    statusLine.firstElementChild,
    statusIcon,
    "status icon precedes its text",
  );
  assertEquals(
    statusIcon.nextElementSibling,
    statusLabel,
    "status text is the icon's separate sibling",
  );
  assertEquals(
    statusLabel.getAttribute("value"),
    status,
    "sync status is the second line",
  );
  assertEquals(
    nameLabel.compareDocumentPosition(statusLabel) &
      Node.DOCUMENT_POSITION_FOLLOWING,
    Node.DOCUMENT_POSITION_FOLLOWING,
    "the name precedes its status in reading order",
  );
}

function assertStatusRendering(
  popup: Element,
  sidebar: Element,
  state: "selected" | "attention" | undefined,
): void {
  const status = popup.querySelector(".workspace-sync-status");
  const icon = popup.querySelector(".workspace-sync-status-icon");
  const sidebarStyle = getComputedStyle(sidebar, "::after");
  assert(sidebarStyle, "the sidebar must have computed styles");
  if (state === undefined) {
    assertEquals(
      popup.querySelector(".workspace-sync-status-line"),
      null,
      "excluded workspaces must not reserve a status row",
    );
    const name = popup.querySelector(".workspace-name");
    const workspaceIcon = popup.querySelector(".workspaceButtonIcon");
    assert(name && workspaceIcon, "the workspace name and icon remain present");
    assert(
      workspaceIcon.getAttribute("src"),
      "the original workspace icon remains loaded",
    );
    for (const element of [name, workspaceIcon]) {
      const bounds = element.getBoundingClientRect();
      const style = getComputedStyle(element);
      assert(
        bounds.width > 0 && bounds.height > 0 && style &&
          style.display !== "none" && style.visibility === "visible",
        "excluded workspace names and icons retain visible geometry",
      );
    }
    assertEquals(
      sidebarStyle.maskImage,
      "none",
      "an excluded sidebar entry has no status mask",
    );
    return;
  }
  assert(
    status && icon,
    "status text and icon must be actual sibling elements",
  );
  const textStyle = getComputedStyle(status);
  assert(textStyle, "the mounted status text must have computed styles");
  const textBounds = status.getBoundingClientRect();
  assert(
    textBounds.width > 0 && textBounds.height > 0 &&
      textStyle.display !== "none" && textStyle.visibility === "visible",
    "each localized status must have a visible text box with production styles",
  );
  const iconStyle = getComputedStyle(icon);
  assert(
    iconStyle && sidebarStyle,
    "both mounted status icons must have computed styles",
  );
  const iconBounds = icon.getBoundingClientRect();
  assert(
    iconStyle.display !== "none" && iconBounds.width > 0 &&
      iconBounds.height > 0,
    "selected and error statuses display their separate icon box",
  );
  for (
    const [surface, mask] of [
      ["popup", iconStyle.maskImage],
      ["sidebar", sidebarStyle.maskImage],
    ]
  ) {
    assert(mask !== "none", `${surface} must have a real status mask`);
    assertEquals(
      mask.includes("chrome://global/skin/icons/warning.svg"),
      state === "attention",
      `${surface} must use the warning mask only for an error`,
    );
  }
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
      const indicator = createRoot((cleanup) => {
        dispose = cleanup;
        const indicator = createWorkspaceSyncIndicator(
          boundary.ctx,
          () => boundary.firstId,
          () => "Alpha",
        );
        createEffect(() => applyWorkspaceSyncIndicator(node, indicator()));
        return indicator;
      });
      await settle();
      assertIndicator(
        node,
        undefined,
        "Alpha",
      );
      assertEquals(
        indicator().status,
        "",
        "excluded workspace has no status caption",
      );
      assertEquals(
        indicator().error,
        null,
        "local status has no visible error",
      );
      boundary.setSelected(new Set([boundary.firstId]));
      await settle();
      assertIndicator(node, "selected", "Alpha — Included in workspace sync");
      assertEquals(indicator().status, "Included in sync", "selected status");
      assertEquals(indicator().error, null, "selected workspace has no error");
      boundary.setErrors(new Map([[boundary.firstId, ERROR]]));
      await settle();
      assertIndicator(
        node,
        "attention",
        `Alpha — Workspace sync error: ${ERROR}`,
      );
      assertEquals(indicator().status, "Sync error", "attention status");
      assertEquals(indicator().error, ERROR, "attention preserves the reason");
      boundary.setErrors(new Map<TWorkspaceID, string>());
      await settle();
      assertIndicator(node, "selected", "Alpha — Included in workspace sync");
      boundary.setSelected(new Set<TWorkspaceID>());
      await settle();
      assertIndicator(
        node,
        undefined,
        "Alpha",
      );
      assertEquals(
        indicator().status,
        "",
        "opt-out removes the status caption",
      );
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
    await activateTestWindow();
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
    popupHost.setAttribute(
      "style",
      "position: fixed; top: 80px; left: 80px; width: 300px;",
    );
    sidebarHost.setAttribute(
      "style",
      "position: fixed; top: 200px; left: 80px; width: 80px;",
    );
    const productionStyles = document.createElement("style");
    productionStyles.textContent = workspacesStyles;
    const previousFocus = document.activeElement;
    document.documentElement.append(productionStyles, popupHost, sidebarHost);
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
      assert(
        sidebar instanceof HTMLElement,
        "the sidebar entry must be a real HTML button target",
      );
      assertEquals(
        sidebar.tabIndex,
        0,
        "the production sidebar entry must be in the keyboard focus order",
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
        status: string,
        name = "Alpha",
      ) => {
        assertIndicator(popup, state, tooltip);
        assertIndicator(sidebar, state, tooltip, "title");
        assertPopupText(popup, name, status);
        assertStatusRendering(popup, sidebar, state);
      };
      assertBoth(
        undefined,
        "Alpha",
        "",
      );
      boundary.setSelected(new Set([boundary.firstId]));
      await settle();
      assertBoth(
        "selected",
        "Alpha — Included in workspace sync",
        "Included in sync",
      );
      boundary.setErrors(new Map([[boundary.firstId, ERROR]]));
      await settle();
      assertBoth(
        "attention",
        `Alpha — Workspace sync error: ${ERROR}`,
        "Sync error",
      );
      const sidebarTooltipId = sidebar.getAttribute(
        "data-workspace-sync-tooltip",
      );
      assert(
        sidebarTooltipId,
        "the production sidebar must bind its focused error tooltip",
      );
      const sidebarTooltip = document.getElementById(sidebarTooltipId) as
        | XULPopupElement
        | null;
      assert(
        sidebarTooltip,
        "the production sidebar's native tooltip must exist",
      );
      sidebar.focus();
      await waitForUI(
        () => document.activeElement === sidebar,
        "the production sidebar entry must actually receive focus",
      );
      await waitForUI(
        () => sidebarTooltip.state === "open",
        "the focused production sidebar must display its actual native error popup",
      );
      assertEquals(
        sidebarTooltip.querySelector(".workspace-sync-focus-reason")
          ?.textContent,
        ERROR,
        "the production sidebar's focused popup preserves the full error reason",
      );
      sidebar.blur();
      await waitForUI(
        () =>
          document.activeElement !== sidebar &&
          sidebarTooltip.state === "closed",
        "blurring the production sidebar must close its error popup",
      );
      boundary.setSelected(new Set<TWorkspaceID>());
      await settle();
      assertBoth(
        undefined,
        "Alpha",
        "",
      );
      boundary.setSelected(new Set([boundary.firstId]));
      await settle();
      assertBoth(
        "attention",
        `Alpha — Workspace sync error: ${ERROR}`,
        "Sync error",
      );
      boundary.setErrors(new Map<TWorkspaceID, string>());
      await settle();
      assertBoth(
        "selected",
        "Alpha — Included in workspace sync",
        "Included in sync",
      );
      boundary.setSelected(new Set<TWorkspaceID>());
      await settle();
      assertBoth(
        undefined,
        "Alpha",
        "",
      );
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
      assertBoth(
        undefined,
        "Beta",
        "",
        "Beta",
      );
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
      assertBoth(
        "selected",
        "Beta — Included in workspace sync",
        "Included in sync",
        "Beta",
      );
      await i18next.changeLanguage("ja-JP");
      setLanguage("ja-JP");
      await settle();
      assertBoth("selected", "Beta — 同期対象", "同期対象", "Beta");
      boundary.setErrors(new Map([[boundary.secondId, ERROR]]));
      await settle();
      assertBoth(
        "attention",
        `Beta — 同期エラー: ${ERROR}`,
        "同期エラー",
        "Beta",
      );
      boundary.setSelected(new Set<TWorkspaceID>());
      await settle();
      assertBoth(undefined, "Beta", "", "Beta");
    } finally {
      disposeComponents();
      popupHost.remove();
      sidebarHost.remove();
      productionStyles.remove();
      if (
        previousFocus instanceof HTMLElement ||
        previousFocus instanceof XULElement
      ) {
        previousFocus.focus();
      }
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
      const indicator = createRoot((cleanup) => {
        dispose = cleanup;
        const indicator = createWorkspaceSyncIndicator(
          boundary.ctx,
          () => boundary.firstId,
          name,
        );
        createEffect(() => applyWorkspaceSyncIndicator(node, indicator()));
        return indicator;
      });
      await settle();
      assertIndicator(node, "selected", "Alpha — Included in workspace sync");
      assertEquals(
        indicator().status,
        "Included in sync",
        "English selected status",
      );
      await i18next.changeLanguage("ja-JP");
      setLanguage("ja-JP");
      await settle();
      assertIndicator(node, "selected", "Alpha — 同期対象");
      assertEquals(
        indicator().status,
        "同期対象",
        "same workspace localizes its status",
      );
      setName("仕事 🌙");
      await settle();
      assertIndicator(node, "selected", "仕事 🌙 — 同期対象");
      boundary.setErrors(new Map([[boundary.firstId, ERROR]]));
      await settle();
      assertIndicator(
        node,
        "attention",
        `仕事 🌙 — 同期エラー: ${ERROR}`,
      );
      assertEquals(indicator().status, "同期エラー", "Japanese error status");
      assertEquals(
        indicator().error,
        ERROR,
        "translation keeps the full error reason",
      );
      boundary.setSelected(new Set<TWorkspaceID>());
      await settle();
      assertIndicator(node, undefined, "仕事 🌙");
      assertEquals(
        indicator().status,
        "",
        "opt-out removes the Japanese status caption",
      );
      assertEquals(
        indicator().error,
        null,
        "opt-out hides a stale error reason",
      );
    } finally {
      dispose();
    }
  });
}

async function testMissingControllerOrWorkspaceIsLocal(): Promise<void> {
  await withEnglish(() => {
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
        assertEquals(
          missingId().error,
          null,
          "missing workspace has no error reason",
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
        assertEquals(
          privateLike().error,
          null,
          "inactive controller hides stale errors",
        );
        assertEquals(
          privateLike().status,
          "",
          "inactive workspace has no status caption",
        );
        assertEquals(
          missingController().status,
          "",
          "missing controller has no status caption",
        );
      } finally {
        dispose();
      }
    });
  });
}

async function testFocusedErrorTooltipUsesNativePopupAndCleansUp(): Promise<
  void
> {
  await withEnglish(async () => {
    await activateTestWindow();
    const boundary = createBoundary();
    const [workspaceId, setWorkspaceId] = createSignal(boundary.firstId);
    const host = document.createXULElement("toolbar");
    host.setAttribute(
      "style",
      "position: fixed; top: 80px; left: 80px; z-index: 2147483647;",
    );
    const node = document.createXULElement("toolbarbutton") as XULElement;
    node.setAttribute("label", "Sync focus fixture");
    node.setAttribute("tabindex", "0");
    const blurTarget = document.createXULElement("toolbarbutton") as XULElement;
    blurTarget.setAttribute("label", "Focus destination");
    blurTarget.setAttribute("tabindex", "0");
    host.append(node, blurTarget);
    const previousFocus = document.activeElement;
    document.documentElement.append(host);
    let dispose = () => {};
    let tooltip: XULPopupElement | null = null;
    try {
      const indicator = createRoot((cleanup) => {
        dispose = cleanup;
        const indicator = createWorkspaceSyncIndicator(
          boundary.ctx,
          workspaceId,
          () => boundary.workspaces.get(workspaceId())?.name ?? "",
        );
        createEffect(() => applyWorkspaceSyncIndicator(node, indicator()));
        bindWorkspaceSyncFocusTooltip(node, indicator);
        return indicator;
      });
      await settle();
      const tooltipId = node.getAttribute("data-workspace-sync-tooltip");
      assert(tooltipId, "the focused node must identify its owned tooltip");
      tooltip = document.getElementById(tooltipId) as XULPopupElement | null;
      assert(tooltip, "the owned tooltip must be in the chrome document");
      assert(
        tooltip.classList.contains("workspace-sync-focus-tooltip"),
        "the helper must create a real native tooltip in the chrome document",
      );
      const nativeTooltip = tooltip;
      const reason = nativeTooltip.querySelector(
        ".workspace-sync-focus-reason",
      );
      assert(reason, "the native tooltip must expose the full error reason");
      let shown = 0;
      let hidden = 0;
      nativeTooltip.addEventListener("popupshown", () => shown++);
      nativeTooltip.addEventListener("popuphidden", () => hidden++);
      const focus = async () => {
        node.focus();
        await waitForUI(
          () => document.activeElement === node,
          "the attached toolbarbutton must really receive keyboard focus",
        );
      };
      const blur = async () => {
        blurTarget.focus();
        await waitForUI(
          () => document.activeElement === blurTarget,
          "focus must really leave the indicator",
        );
      };
      const expectOpen = async () => {
        await waitForUI(
          () => nativeTooltip.state === "open",
          "an error reason must become a visible native popup on focus",
        );
        assert(
          nativeTooltip.getBoundingClientRect().width > 0,
          "error popup has visible geometry",
        );
        assertEquals(
          reason.textContent,
          ERROR,
          "focus displays the complete error reason",
        );
      };
      const expectClosed = async (message: string) => {
        await waitForUI(() => nativeTooltip.state === "closed", message);
      };

      await focus();
      await settle();
      assertEquals(
        nativeTooltip.state,
        "closed",
        "local focus must not open an error popup",
      );
      assertEquals(shown, 0, "local focus must not emit popupshown");
      assertIndicator(
        node,
        undefined,
        "Alpha",
      );
      boundary.setSelected(new Set([boundary.firstId]));
      await settle();
      assertEquals(
        nativeTooltip.state,
        "closed",
        "normal sync selection must not open an error popup",
      );
      assertIndicator(node, "selected", "Alpha — Included in workspace sync");

      await blur();
      boundary.setErrors(new Map([[boundary.firstId, ERROR]]));
      await focus();
      await expectOpen();
      assertEquals(
        shown,
        1,
        "native popupshown confirms the initial visible popup",
      );
      assertIndicator(
        node,
        "attention",
        `Alpha — Workspace sync error: ${ERROR}`,
      );
      await blur();
      await expectClosed("blur must hide the error reason");
      assertEquals(hidden, 1, "blur emits a native popuphidden event");

      await focus();
      await expectOpen();
      node.dispatchEvent(
        new KeyboardEvent("keydown", { key: "Escape", bubbles: true }),
      );
      await expectClosed("Escape must hide the focused error reason");
      assertEquals(
        document.activeElement,
        node,
        "Escape need not remove keyboard focus",
      );
      await settle();
      assertEquals(
        nativeTooltip.state,
        "closed",
        "the dismissed reason stays hidden while focus remains",
      );

      await blur();
      await focus();
      await expectOpen();
      boundary.setErrors(new Map<TWorkspaceID, string>());
      await expectClosed("clearing the error must close its focused popup");
      assertEquals(
        indicator().error,
        null,
        "cleared error leaves no exposed reason",
      );
      assertIndicator(node, "selected", "Alpha — Included in workspace sync");

      await blur();
      boundary.setErrors(new Map([[boundary.firstId, ERROR]]));
      await focus();
      await expectOpen();
      setWorkspaceId(boundary.secondId);
      await expectClosed(
        "switching to a local workspace must hide the prior workspace's error",
      );
      assertIndicator(node, undefined, "Beta");
      assertEquals(
        indicator().error,
        null,
        "a local workspace must not inherit the previous error",
      );

      await blur();
      setWorkspaceId(boundary.firstId);
      await focus();
      await expectOpen();
      boundary.setSelected(new Set<TWorkspaceID>());
      await expectClosed(
        "opting out must hide a stale error even while focused",
      );
      assertEquals(
        indicator().error,
        null,
        "opt-out removes the exposed reason",
      );
      assertIndicator(
        node,
        undefined,
        "Alpha",
      );

      await blur();
      boundary.setSelected(new Set([boundary.firstId]));
      await focus();
      await expectOpen();
      dispose();
      assert(
        !nativeTooltip.isConnected,
        "disposing its Solid owner removes the native tooltip",
      );
      assertEquals(
        document.getElementById(tooltipId),
        null,
        "disposed tooltip cannot remain in the document",
      );
      await blur();
      await focus();
      await settle();
      assertEquals(
        document.getElementById(tooltipId),
        null,
        "disposed focus listeners cannot recreate a popup",
      );
    } finally {
      dispose();
      if (tooltip?.isConnected) tooltip.hidePopup();
      host.remove();
      if (
        previousFocus instanceof HTMLElement ||
        previousFocus instanceof XULElement
      ) {
        previousFocus.focus();
      }
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
    {
      name:
        "focused sync errors use a native popup and clean up on blur, Escape, state changes and disposal",
      fn: testFocusedErrorTooltipUsesNativePopupAndCleansUp,
    },
  ]);
}
