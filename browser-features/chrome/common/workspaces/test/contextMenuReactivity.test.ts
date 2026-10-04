// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { act } from "preact/test-utils";
import { createRoot } from "@nora/preact-xul/lifetime";
import { WorkspacesPopupContextMenu } from "../contextMenu/popupSet.tsx";
import { setWorkspacesDataStore, workspacesDataStore } from "../data/data.ts";
import type { WorkspacesService } from "../workspacesService.ts";
import type { TWorkspaceID } from "../utils/type.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

async function testRapidReopenRetainsCommands(): Promise<void> {
  const menuId = "workspaces-toolbar-item-context-menu";
  const originalMenu = document.getElementById(menuId);
  const originalPopupNode = Reflect.get(document, "popupNode");
  const originalOrder = [...workspacesDataStore.order];
  const first = "99999999-0000-4000-8000-000000000001" as TWorkspaceID;
  const middle = "99999999-0000-4000-8000-000000000002" as TWorkspaceID;
  const last = "99999999-0000-4000-8000-000000000003" as TWorkspaceID;
  const fixture = document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "div",
  );
  const buttons = [first, middle, last].map((id) => {
    // Sidebar buttons live inside wrappers rather than adjacent to each other.
    const wrapper = document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "div",
    );
    const button = document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "div",
    );
    button.id = `workspace-${id}`;
    wrapper.appendChild(button);
    fixture.appendChild(wrapper);
    return button;
  });
  document.documentElement.appendChild(fixture);
  let disposed = () => {};
  let deleted: TWorkspaceID | null = null;
  const ctx = {
    isWorkspaceID: (id: string) =>
      [first, middle, last].includes(id as TWorkspaceID),
    deleteWorkspace: (id: TWorkspaceID) => {
      deleted = id;
    },
  } as unknown as WorkspacesService;
  try {
    originalMenu?.removeAttribute("id");
    setWorkspacesDataStore("order", [...originalOrder, first, middle, last]);
    await act(() => {
      createRoot((dispose) => {
        disposed = dispose;
        new WorkspacesPopupContextMenu(ctx);
      });
    });
    const popup = document.getElementById(menuId);
    assert(popup, "fixture context menu was inserted");
    const show = (target: Element) => {
      Reflect.set(document, "popupNode", target);
      return popup.dispatchEvent(
        new Event("popupshowing", { cancelable: true }),
      );
    };
    const command = (name: string) =>
      popup.querySelector(
        `[data-floorp-context-menu-key="floorp.workspaces.${name}"]`,
      );
    await act(() => {
      show(buttons[1]);
    });
    const deleteItem = command("delete");
    assert(deleteItem, "first opening contains commands");
    const moveUp = command("move-up");
    const moveDown = command("move-down");
    assert(moveUp && moveDown, "both movement commands are present");
    assertEquals(
      Reflect.get(moveUp, "disabled"),
      false,
      "middle sidebar workspace can move up",
    );
    assertEquals(
      Reflect.get(moveDown, "disabled"),
      false,
      "middle sidebar workspace can move down",
    );

    await act(() => {
      popup.dispatchEvent(new Event("popuphiding"));
      show(buttons[2]);
    });
    assertEquals(
      command("delete"),
      deleteItem,
      "rapid reopen preserves attached managed commands",
    );
    assertEquals(
      Reflect.get(moveDown, "disabled"),
      true,
      "reopening uses the latest workspace bounds",
    );
    deleteItem.dispatchEvent(new Event("command"));
    assertEquals(
      deleted,
      last,
      "the retained command acts on the latest target",
    );

    await act(() => {
      popup.dispatchEvent(new Event("popuphiding"));
      show(buttons[2]);
    });
    assert(
      command("delete"),
      "same-target rapid reopen does not empty the menu",
    );
    await act(() => {
      assertEquals(show(fixture), false, "invalid targets cancel the opening");
    });
    assertEquals(
      command("delete"),
      null,
      "invalid targets cannot retain a previous workspace command",
    );
  } finally {
    disposed();
    fixture.remove();
    originalMenu?.setAttribute("id", menuId);
    Reflect.set(document, "popupNode", originalPopupNode);
    setWorkspacesDataStore("order", originalOrder);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("contextMenuReactivity.test.ts", [{
    name:
      "workspace context menu survives batched reopen and tracks the current target",
    fn: testRapidReopenRetainsCommands,
  }]);
}
