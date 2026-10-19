// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import type { StatusbarTestWindow } from "../../statusbar/test/types.ts";
import type { WorkspaceSyncAPI } from "../workspacesSyncTypes.ts";
import type {
  TWorkspace,
  TWorkspaceID,
  TWorkspacesStoreData,
} from "../utils/type.ts";
import { WORKSPACE_SYNC_CHANGED } from "#modules/common/workspace-sync.ts";
import {
  WORKSPACE_DATA_PREF_NAME,
  WORKSPACE_ENABLED_PREF_NAME,
  WORKSPACE_PENDING_EXIT_PREF_NAME,
} from "../utils/workspaces-static-names.ts";
import type { TestWindow } from "./workspace-last-tab-test-types.ts";
import { waitFor } from "./workspace-last-tab-test-utils.ts";

async function testRealWindowsKeepIndependentIndicators(): Promise<void> {
  assert(
    Services.prefs.getBoolPref(WORKSPACE_ENABLED_PREF_NAME, true) &&
      (window as unknown as TestWindow).workspacesFuncs,
    "this real-window regression requires an initialized workspace-enabled runner",
  );
  const { FloorpWorkspacesSync: api } = ChromeUtils.importESModule(
    "resource://noraneko/modules/workspaces/FloorpWorkspacesSync.sys.mjs",
  ) as { FloorpWorkspacesSync: WorkspaceSyncAPI };
  await api.ready();
  const originalSelected = api.isSelected;
  const originalError = api.errorFor;
  for (const key of ["isSelected", "errorFor"]) {
    assert(
      Object.getOwnPropertyDescriptor(api, key)?.writable,
      "the native API must allow a scoped, restorable test fixture",
    );
  }
  const originalPreference = Services.prefs.getStringPref(
    WORKSPACE_DATA_PREF_NAME,
  );
  const hadPreference = Services.prefs.prefHasUserValue(
    WORKSPACE_DATA_PREF_NAME,
  );
  const hadPendingExit = Services.prefs.prefHasUserValue(
    WORKSPACE_PENDING_EXIT_PREF_NAME,
  );
  const pendingExit = Services.prefs.getBoolPref(
    WORKSPACE_PENDING_EXIT_PREF_NAME,
    false,
  );
  const data = JSON.parse(
    originalPreference,
    (key, value) => key === "data" ? new Map(value) : value,
  ) as TWorkspacesStoreData;
  const ids = [0, 1].map(() =>
    Services.uuid.generateUUID().toString().slice(1, -1) as TWorkspaceID
  );
  const names = ["Window sync Alpha", "Window sync Beta"];
  const selected = new Set<string>([ids[0]]);
  const errors = new Set<string>();
  const owned: TestWindow[] = [];
  const notify = () =>
    Services.obs.notifyObservers(
      null as unknown as nsISupports,
      WORKSPACE_SYNC_CHANGED,
    );
  const nodeFor = (win: TestWindow) =>
    win.document.getElementById("workspaces-toolbar-button") as
      | XULElement
      | null;
  const check = async (
    win: TestWindow,
    index: number,
    state: string | null,
  ) => {
    await waitFor(
      () =>
        nodeFor(win)?.getAttribute("data-workspace-sync") === state &&
        !!nodeFor(win)?.getAttribute("aria-label")?.startsWith(names[index]),
      "the real toolbar must follow its own window's workspace",
    );
    const node = nodeFor(win)!;
    assertEquals(
      node.getAttribute("tooltiptext"),
      node.getAttribute("aria-label"),
      "visible and accessible descriptions agree",
    );
    return node;
  };
  const close = async (win: TestWindow) => {
    if (win.closed) return;
    let unloaded = false;
    const onUnload = () => unloaded = true;
    win.addEventListener("unload", onUnload, { once: true });
    try {
      win.close();
      await waitFor(
        () => win.closed && unloaded,
        "the owned window completes native unload",
      );
    } finally {
      win.removeEventListener("unload", onUnload);
    }
  };
  try {
    // Only these new IDs use fixture state. Every other native query passes through;
    // this fixture never selects or captures synthetic workspaces in the ledger.
    api.isSelected = (id) =>
      ids.includes(id as TWorkspaceID)
        ? selected.has(id)
        : originalSelected.call(api, id);
    api.errorFor = (id) =>
      ids.includes(id as TWorkspaceID)
        ? errors.has(id) ? "invalid-workspace" : null
        : originalError.call(api, id);
    const workspaces = new Map(
      data.data as unknown as Map<TWorkspaceID, TWorkspace>,
    );
    ids.forEach((id, index) =>
      workspaces.set(id, {
        name: names[index],
        icon: "floorp-icon:v1:book",
        userContextId: 0,
        isDefault: null,
        isSelected: null,
      })
    );
    Services.prefs.setStringPref(
      WORKSPACE_DATA_PREF_NAME,
      JSON.stringify({
        ...data,
        data: [...workspaces],
        order: [...data.order, ...ids],
      }),
    );
    for (let index = 0; index < 2; index++) {
      const win = (window as unknown as StatusbarTestWindow)
        .OpenBrowserWindow() as unknown as TestWindow;
      owned.push(win);
      await waitFor(
        () =>
          !!win.gBrowserInit?.delayedStartupFinished &&
          !!win.workspacesFuncs && !!nodeFor(win),
        "the owned window initializes the production workspace toolbar",
        30_000,
      );
      win.workspacesFuncs!.changeWorkspace(ids[index]);
      await waitFor(
        () => win.workspacesFuncs?.getSelectedWorkspaceID() === ids[index],
        "the owned window selects its fixture workspace",
      );
    }
    notify();
    await check(owned[0], 0, "selected");
    await check(owned[1], 1, null);
    selected.clear();
    selected.add(ids[1]);
    notify();
    await check(owned[0], 0, null);
    const survivorNode = await check(owned[1], 1, "selected");
    const tooltipId = survivorNode.getAttribute("data-workspace-sync-tooltip");
    assert(tooltipId, "the surviving window owns a focus tooltip");
    await close(owned[0]);
    errors.add(ids[1]);
    notify();
    await check(owned[1], 1, "attention");
    assertEquals(
      survivorNode.getAttribute("data-workspace-sync-tooltip"),
      tooltipId,
      "closing a sibling preserves the survivor's binding",
    );
    const tooltip = owned[1].document.getElementById(tooltipId) as
      | XULPopupElement
      | null;
    assert(tooltip, "the survivor's tooltip remains attached");
    const navigator = Reflect.get(owned[1], "ToolbarKeyboardNavigator");
    assert(
      navigator && typeof navigator === "object",
      "native toolbar navigation exists",
    );
    const focusButton = Reflect.get(navigator, "_focusButton");
    assert(
      typeof focusButton === "function",
      "native toolbar focus is available",
    );
    focusButton.call(navigator, survivorNode);
    await waitFor(
      () =>
        owned[1].document.activeElement === survivorNode &&
        tooltip.state === "open",
      "the surviving window opens its native error explanation on focus",
    );
    const reason = tooltip.querySelector(".workspace-sync-focus-reason")
      ?.textContent;
    assert(
      reason && reason.includes("256"),
      "focus exposes the full localized error",
    );
    assert(
      survivorNode.getAttribute("aria-label")?.includes(reason),
      "the accessible description includes the same complete reason",
    );
    errors.clear();
    notify();
    await check(owned[1], 1, "selected");
    await waitFor(
      () => tooltip.state === "closed",
      "recovery hides the explanation",
    );
  } finally {
    api.isSelected = originalSelected;
    api.errorFor = originalError;
    notify();
    try {
      const closed = await Promise.allSettled(owned.map(close));
      assert(
        closed.every((result) => result.status === "fulfilled"),
        "all owned windows close",
      );
    } finally {
      if (hadPreference) {
        Services.prefs.setStringPref(
          WORKSPACE_DATA_PREF_NAME,
          originalPreference,
        );
      } else Services.prefs.clearUserPref(WORKSPACE_DATA_PREF_NAME);
      if (hadPendingExit) {
        Services.prefs.setBoolPref(
          WORKSPACE_PENDING_EXIT_PREF_NAME,
          pendingExit,
        );
      } else Services.prefs.clearUserPref(WORKSPACE_PENDING_EXIT_PREF_NAME);
      window.focus();
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("workspaceSyncIndicatorWindows.test.ts", [{
    name:
      "real windows keep independent sync indicators after a sibling closes",
    fn: testRealWindowsKeepIndependentIndicators,
  }]);
}
