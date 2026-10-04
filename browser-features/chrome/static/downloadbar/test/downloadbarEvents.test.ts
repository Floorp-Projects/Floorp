// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { h, safeRender } from "@nora/preact-xul";
import { DonwloadBar } from "../downloadbar.tsx";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

function testNativeDownloadHandlers(): void {
  const container = document.createElement("div");
  const restored: (() => void)[] = [];
  const received: { owner: object; method: string; event: Event }[] = [];
  const handlers = [
    [
      DownloadsView,
      "onDownloadMouseOver",
      "#downloadsListBox",
      new MouseEvent("mouseover"),
    ],
    [
      DownloadsView,
      "onDownloadMouseOut",
      "#downloadsListBox",
      new MouseEvent("mouseout"),
    ],
    [
      DownloadsView,
      "onDownloadContextMenu",
      "#downloadsListBox",
      new MouseEvent("contextmenu"),
    ],
    [
      DownloadsView,
      "onDownloadDragStart",
      "#downloadsListBox",
      new DragEvent("dragstart"),
    ],
    [
      DownloadsSummary,
      "onKeyDown",
      "#downloadsSummary",
      new KeyboardEvent("keydown", { key: "Enter" }),
    ],
    [DownloadsSummary, "onClick", "#downloadsSummary", new MouseEvent("click")],
    [
      DownloadsPanel,
      "showDownloadsHistory",
      "#downloadIcon",
      new Event("command"),
    ],
    [
      DownloadsPanel,
      "showDownloadsHistory",
      "#downloadsHistory",
      new Event("command"),
    ],
  ] as const;
  let dispose: (() => void) | undefined;
  try {
    const patched = new Set<string>();
    for (const [owner, method] of handlers) {
      if (patched.has(method)) continue;
      patched.add(method);
      const descriptor = Object.getOwnPropertyDescriptor(owner, method);
      Object.defineProperty(owner, method, {
        configurable: true,
        value(this: object, event: Event) {
          received.push({ owner: this, method, event });
        },
      });
      restored.push(() => {
        if (descriptor) Object.defineProperty(owner, method, descriptor);
        else Reflect.deleteProperty(owner, method);
      });
    }
    dispose = safeRender(h(DonwloadBar, {}), container);
    for (const [owner, method, selector, event] of handlers) {
      const target = container.querySelector(selector);
      assert(target, `download bar renders ${selector}`);
      const before = received.length;
      target.dispatchEvent(event);
      assertEquals(
        received.length,
        before + 1,
        `${event.type} invokes exactly one handler`,
      );
      const call = received[before];
      assertEquals(call.owner, owner, "native handler keeps its receiver");
      assertEquals(call.method, method, "native handler matches the control");
      if (owner !== DownloadsPanel) {
        assertEquals(
          call.event,
          event,
          "native handler receives the original event",
        );
      }
    }
    dispose();
    assertEquals(
      container.childNodes.length,
      0,
      "disposing removes the download bar",
    );
  } finally {
    dispose?.();
    for (const restore of restored.reverse()) restore();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("downloadbarEvents.test.ts", [
    {
      name: "download bar forwards native mouse, keyboard and command events",
      fn: testNativeDownloadHandlers,
    },
  ]);
}
