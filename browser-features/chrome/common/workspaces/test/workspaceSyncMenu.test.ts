// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { render } from "@nora/solid-xul";
import { createSignal } from "solid-js";
import { ContextMenu } from "../contextMenu/contextMenu.tsx";
import type { TWorkspaceID } from "../utils/type.ts";
import type { WorkspacesService } from "../workspacesService.ts";
import type { WorkspaceSyncMenuItem } from "./workspace-sync-menu-test-types.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

async function testNativeSyncMenuStateTracksSignals(): Promise<void> {
  const [available, setAvailable] = createSignal(true);
  const [selected, setSelected] = createSignal(false);
  const [enabled, setEnabled] = createSignal(true);
  const selections: boolean[] = [];
  const workspaceId = "24be4056-599f-466b-9d3d-fd33ed287ac8" as TWorkspaceID;
  // Only the sync command is exercised. The real component and renderer retain
  // ownership of every XUL attribute; the service boundary supplies state only.
  const ctx = {
    syncController: {
      isAvailable: available,
      isEnabled: enabled,
      isSelected: selected,
      errorText: () => null,
      select: (_id: TWorkspaceID, value: boolean) => {
        selections.push(value);
        return Promise.resolve();
      },
    },
  } as unknown as WorkspacesService;
  const host = document.createXULElement("menupopup");
  document.documentElement.appendChild(host);
  let dispose = () => {};
  try {
    dispose = render(
      () =>
        ContextMenu({
          disableBefore: false,
          disableAfter: false,
          contextWorkspaceId: workspaceId,
          ctx,
        }),
      host,
    );
    await Promise.resolve();
    const item = host.querySelector<WorkspaceSyncMenuItem>(
      '[data-floorp-context-menu-key="floorp.workspaces.sync"]',
    );
    assert(item, "the production ContextMenu must render the sync menu item");
    assertEquals(item.localName, "menuitem", "the command must be native XUL");
    assertEquals(
      item.namespaceURI,
      "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
      "the renderer must create a native XUL menu item",
    );
    assertEquals(
      item.disabled,
      false,
      "an available controller must leave the native command enabled",
    );
    assert(
      !item.hasAttribute("disabled"),
      "an enabled native command must omit the disabled attribute",
    );
    assert(
      !item.hasAttribute("checked"),
      "an unselected workspace must omit the checked attribute",
    );

    const normalHelp = item.getAttribute("tooltiptext");
    assert(normalHelp, "an unselected workspace has normal sync help");
    setEnabled(false);
    await Promise.resolve();
    assertEquals(
      item.getAttribute("tooltiptext"),
      normalHelp,
      "an unselected workspace must not advertise an unavailable resume action",
    );
    setEnabled(true);
    await Promise.resolve();

    setSelected(true);
    await Promise.resolve();
    assertEquals(
      item.getAttribute("checked"),
      "true",
      "selection changes must reach the rendered menu item",
    );

    setEnabled(false);
    await Promise.resolve();
    assert(!item.hasAttribute("checked"), "paused transport is not checked");
    assertEquals(item.disabled, false, "paused transport can be resumed");
    assert(
      /Resume Workspace Sync|ワークスペース同期を再開/u.test(
        item.getAttribute("label") ?? "",
      ),
      "a selected but paused workspace exposes the resume action",
    );
    item.dispatchEvent(new Event("command", { bubbles: true }));
    await Promise.resolve();
    assertEquals(
      selections.at(-1),
      true,
      "resume reselects, rather than opts out",
    );
    setEnabled(true);
    await Promise.resolve();
    assertEquals(item.getAttribute("checked"), "true", "resumed selection");

    setSelected(false);
    await Promise.resolve();
    assert(
      !item.hasAttribute("checked"),
      "deselecting must remove checked instead of serializing false",
    );

    setAvailable(false);
    await Promise.resolve();
    assertEquals(item.disabled, true, "an unavailable controller must disable");
    setAvailable(true);
    await Promise.resolve();
    assert(
      !item.hasAttribute("disabled"),
      "becoming available must remove the native disabled attribute",
    );
    assertEquals(item.disabled, false, "the native command must enable again");
  } finally {
    dispose();
    host.remove();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("workspaceSyncMenu.test.ts", [
    {
      name: "production sync menu tracks native boolean state reactively",
      fn: testNativeSyncMenuStateTracksSignals,
    },
  ]);
}
