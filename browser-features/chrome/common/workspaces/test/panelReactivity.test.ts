// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { h } from "preact";
import { act } from "preact/test-utils";
import { safeRender } from "@nora/preact-xul";
import Workspaces from "../index.ts";
import { WorkspacesPanels } from "../toolbar/workspaces-panels.tsx";
import { configStore, enabled, setConfigStore } from "../data/config.ts";
import { setWorkspacesDataStore, workspacesDataStore } from "../data/data.ts";
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

async function testVisibilityAndReordering(): Promise<void> {
  const originalConfig = { ...configStore };
  const originalEnabled = enabled.value;
  const originalData = { ...workspacesDataStore };
  const originalCtx = Workspaces.windowWorkspacesMap.get(window);
  const fixture = document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "div",
  );
  document.documentElement.appendChild(fixture);
  let dispose = () => {};
  const first = "99999999-0000-4000-8000-000000000001" as TWorkspaceID;
  const second = "99999999-0000-4000-8000-000000000002" as TWorkspaceID;
  const createWorkspace = (name: string): TWorkspace => ({
    name,
    userContextId: 0,
    isSelected: null,
    isDefault: null,
  });
  const ctx = {
    iconCtx: {
      getWorkspaceIconUrl: () => "chrome://branding/content/icon32.png",
    },
  } as unknown as WorkspacesService;
  try {
    Workspaces.windowWorkspacesMap.set(window, ctx);
    enabled.value = true;
    setConfigStore("manageOnBms", false);
    await act(() => {
      dispose = safeRender(
        () => h(WorkspacesPanels, { ctx: undefined }),
        fixture,
      );
    });
    assertEquals(
      fixture.querySelector("#workspaces-panel-sidebar-section"),
      null,
      "disabled panels start hidden",
    );
    await act(() => setConfigStore("manageOnBms", true));
    assert(
      fixture.querySelector("#workspaces-panel-sidebar-section"),
      "enabling sidebar management resolves the service again",
    );

    await act(() => {
      const data = new Map(
        workspacesDataStore.data as unknown as Map<TWorkspaceID, TWorkspace>,
      );
      data.set(first, createWorkspace("First"));
      data.set(second, createWorkspace("Second"));
      setWorkspacesDataStore({
        data: data as unknown as TWorkspacesStoreData["data"],
        order: [...originalData.order, first, second],
      });
    });
    const firstNode = fixture.querySelector(`#workspace-${first}`);
    const secondNode = fixture.querySelector(`#workspace-${second}`);
    assert(firstNode && secondNode, "both fixture workspaces render");
    firstNode.setAttribute("drag-over", "true");
    await act(() =>
      setWorkspacesDataStore("order", [...originalData.order, second, first])
    );
    assertEquals(
      fixture.querySelector(`#workspace-${first}`),
      firstNode,
      "reordering retains each workspace DOM node",
    );
    assertEquals(
      fixture.querySelector(`#workspace-${second}`),
      secondNode,
      "the other workspace does not inherit a reused node",
    );
    assert(
      !secondNode.hasAttribute("drag-over"),
      "drag state never transfers to another workspace",
    );

    await act(() => setConfigStore("manageOnBms", false));
    await act(() => setConfigStore("manageOnBms", true));
    assert(
      fixture.querySelector(`#workspace-${first}`),
      "repeated visibility changes restore workspace buttons",
    );
  } finally {
    dispose();
    fixture.remove();
    if (originalCtx) Workspaces.windowWorkspacesMap.set(window, originalCtx);
    else Workspaces.windowWorkspacesMap.delete(window);
    setWorkspacesDataStore(originalData);
    setConfigStore(originalConfig);
    enabled.value = originalEnabled;
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("panelReactivity.test.ts", [{
    name:
      "workspace panels recover after enabling and preserve identity when reordered",
    fn: testVisibilityAndReordering,
  }]);
}
