// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { act, useState } from "react";
import { createRoot } from "react-dom/client";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import { useGestureColorDraft } from "../../../src/app/gesture/useGestureColorDraft.ts";

async function testDraftDuringSave() {
  const host = document.createElement("div");
  document.body.append(host);
  const root = createRoot(host);
  const resolvers: Array<(value: boolean) => void> = [];
  const writes: string[] = [];
  let updateExternal: ((value: string) => void) | undefined;
  function Probe() {
    const [committed, setCommitted] = useState("#123");
    updateExternal = setCommitted;
    const [draft, setDraft] = useGestureColorDraft(committed, (value) => {
      writes.push(value);
      return new Promise<boolean>((resolve) => resolvers.push(resolve));
    });
    return (
      <input
        value={draft}
        onChange={(event) => setDraft(event.currentTarget.value)}
      />
    );
  }
  try {
    await act(() => root.render(<Probe />));
    const input = host.querySelector("input");
    assert(input, "color input exists");
    input.focus();
    const setter = Object.getOwnPropertyDescriptor(
      HTMLInputElement.prototype,
      "value",
    )?.set;
    assert(setter, "native value setter exists");
    const insert = async (value: string, caret: number) => {
      await act(() => {
        setter.call(input, value);
        input.setSelectionRange(caret, caret);
        input.dispatchEvent(new Event("input", { bubbles: true }));
      });
    };
    await insert("#1023", 3);
    assertEquals(
      input.value,
      "#1023",
      "first edit remains while save is pending",
    );
    assertEquals(input.selectionStart, 3, "first edit caret remains in place");
    await insert("#10923", 4);
    assertEquals(
      input.value,
      "#10923",
      "second edit remains while first save is pending",
    );
    assertEquals(input.selectionStart, 4, "second edit caret remains in place");
    assertEquals(
      writes.join(","),
      "#1023,#10923",
      "both changes reach persistence in order",
    );
    await act(async () => {
      updateExternal?.("#1023");
      resolvers[0](true);
      await Promise.resolve();
    });
    assertEquals(
      input.value,
      "#10923",
      "older completion does not replace newer draft",
    );
    await act(async () => {
      updateExternal?.("#10923");
      resolvers[1](true);
      await Promise.resolve();
    });
    assertEquals(input.value, "#10923", "latest committed value remains");
    await insert("#109923", 5);
    await act(() => updateExternal?.("#456789"));
    assertEquals(
      input.value,
      "#109923",
      "external changes do not erase an active draft",
    );
    await act(async () => {
      resolvers[2](false);
      await Promise.resolve();
    });
    assertEquals(
      input.value,
      "#456789",
      "failed latest save restores the latest external value",
    );
    await act(() => updateExternal?.("#abcdef"));
    assertEquals(
      input.value,
      "#abcdef",
      "external value changes update an idle draft",
    );
  } finally {
    await act(() => root.unmount());
    host.remove();
  }
}
export async function runAllTests(): Promise<void> {
  await runTests("gestureColorDraft.test.tsx", [{
    name: "pending saves preserve color draft and caret",
    fn: testDraftDuringSave,
  }]);
}
