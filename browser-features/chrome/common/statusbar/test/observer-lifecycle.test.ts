// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createRoot } from "solid-js";
import { StatusBarManager } from "../statusbar-manager.tsx";
import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";

const pref = "noraneko.statusbar.enable";
const settle = () => new Promise<void>((resolve) => setTimeout(resolve, 0));

type Fixture = {
  setEnabled: (enabled: boolean) => void;
  panel: HTMLElement;
  text: HTMLElement;
  label: HTMLElement;
  writes: () => number;
  dispose: () => void;
};

// Keep native MutationObservers and Solid ownership. Chrome stubs exist only
// during synchronous manager calls; every awaited delivery uses restored globals.
async function withFixture(
  run: (fixture: Fixture) => Promise<void>,
  missing: string[] = [],
): Promise<void> {
  const panel = document.createElement("div");
  const text = document.createElement("div");
  const label = document.createElement("span");
  panel.appendChild(label);
  const nodes = new Map<string, HTMLElement>([
    ["statuspanel", panel],
    ["status-text", text],
    ["statuspanel-label", label],
    ["nora-statusbar", document.createElement("div")],
    ["appcontent", document.createElement("div")],
  ]);
  for (const id of missing) nodes.delete(id);
  const prefObservers = new Set<() => void>();
  let enabled = true;
  let writes = 0;
  let unregisters = 0;
  let rootDispose = () => {};
  const textObserver = new MutationObserver((records) => {
    writes += records.length;
  });
  textObserver.observe(text, { attributes: true, attributeFilter: ["hidden"] });
  function withChromeStubs<T>(run: () => T): T {
    const restorers: (() => void)[] = [];
    const checks: (() => void)[] = [];
    function replace(target: object, key: PropertyKey, value: unknown): void {
      const descriptor = Object.getOwnPropertyDescriptor(target, key);
      Object.defineProperty(target, key, { configurable: true, value });
      restorers.push(() => {
        if (descriptor) Object.defineProperty(target, key, descriptor);
        else Reflect.deleteProperty(target, key);
      });
      checks.push(() => {
        const restored = Object.getOwnPropertyDescriptor(target, key);
        assertEquals(
          !!restored,
          !!descriptor,
          `${String(key)} ownership restored`,
        );
        for (
          const field of [
            "value",
            "get",
            "set",
            "writable",
            "enumerable",
            "configurable",
          ] as const
        ) {
          assertEquals(
            restored?.[field],
            descriptor?.[field],
            `${String(key)} descriptor ${field} restored`,
          );
        }
      });
    }
    try {
      // Materialize lazy browser globals before saving their descriptors.
      void Services;
      void globalThis.CustomizableUI;
      replace(globalThis, "Services", {
        prefs: {
          getBoolPref: () => enabled,
          setBoolPref: (name: string, value: boolean) => {
            assertEquals(name, pref, "only fixture preference may be written");
            if (enabled === value) return;
            enabled = value;
            for (const observer of prefObservers) observer();
          },
          addObserver: (_name: string, observer: () => void) => {
            prefObservers.add(observer);
          },
          removeObserver: (_name: string, observer: () => void) => {
            prefObservers.delete(observer);
          },
        },
      });
      replace(globalThis, "CustomizableUI", {
        TYPE_TOOLBAR: "toolbar",
        registerArea: () => {},
        registerToolbarNode: () => {},
        unregisterArea: () => unregisters++,
      });
      replace(globalThis, "gFloorp", {});
      replace(
        document,
        "getElementById",
        (id: string) => nodes.get(id) ?? null,
      );
      replace(document, "querySelector", (selector: string) => {
        return nodes.get(selector.slice(1)) ?? null;
      });
      return run();
    } finally {
      for (const restore of restorers.reverse()) restore();
      for (const check of checks) check();
    }
  }
  const dispose = () => withChromeStubs(rootDispose);
  try {
    const manager = withChromeStubs(() =>
      createRoot((dispose) => {
        rootDispose = dispose;
        const manager = new StatusBarManager();
        manager.init();
        return manager;
      })
    );
    await settle();
    await run({
      setEnabled: (enabled) =>
        withChromeStubs(() => manager.setShowStatusBar(enabled)),
      panel,
      text,
      label,
      writes: () => writes,
      dispose,
    });
  } finally {
    try {
      dispose();
      await settle();
      assertEquals(
        prefObservers.size,
        0,
        "owner disposal removes pref observer",
      );
      assertEquals(
        unregisters,
        1,
        "owner disposal unregisters fixture UI area",
      );
    } finally {
      textObserver.disconnect();
    }
  }
}

async function testToggleReleasesPreviousObserver(): Promise<void> {
  await withFixture(async ({ setEnabled, panel, text, label, writes }) => {
    assertEquals(label.parentNode, text, "ON moves the original label");
    panel.setAttribute("inactive", "true");
    await settle();
    assertEquals(
      text.getAttribute("hidden"),
      "true",
      "ON receives native mutation",
    );
    assertEquals(writes(), 1, "one ON observer writes hidden once");
    panel.removeAttribute("inactive");
    await settle();
    assertEquals(text.hasAttribute("hidden"), false, "ON handles active panel");

    for (let cycle = 0; cycle < 3; cycle++) {
      setEnabled(false);
      await settle();
      assertEquals(label.parentNode, panel, "OFF returns the original label");
      text.setAttribute("hidden", "sentinel");
      await settle();
      const beforeOff = writes();
      panel.setAttribute("inactive", "true");
      await settle();
      assertEquals(writes(), beforeOff, "OFF must not write status text");
      assertEquals(
        text.getAttribute("hidden"),
        "sentinel",
        "OFF has no stale callback",
      );

      setEnabled(true);
      await settle();
      assertEquals(label.parentNode, text, "ON again moves the same label");
      const beforeOn = writes();
      panel.setAttribute("data-probe", String(cycle));
      await settle();
      assertEquals(
        writes() - beforeOn,
        1,
        "ON again has exactly one callback write",
      );
      assertEquals(text.getAttribute("hidden"), "true", "ON again responds");
    }
  });
}

async function testOffDiscardsQueuedMutation(): Promise<void> {
  await withFixture(async ({ setEnabled, panel, text, writes }) => {
    panel.setAttribute("inactive", "true");
    setEnabled(false);
    await settle();
    assertEquals(writes(), 0, "OFF discards an undelivered record");
    assertEquals(
      text.hasAttribute("hidden"),
      false,
      "queued OFF callback did not run",
    );
  });
}

async function testReenableDoesNotAccumulateObservers(): Promise<void> {
  await withFixture(async ({ setEnabled, panel, writes }) => {
    panel.setAttribute("inactive", "true");
    await settle();
    for (let cycle = 0; cycle < 3; cycle++) {
      setEnabled(false);
      setEnabled(true);
      await settle();
      const before = writes();
      panel.setAttribute("data-probe", String(cycle));
      await settle();
      assertEquals(
        writes() - before,
        1,
        "each reenable has one callback write",
      );
    }
  });
}

async function testDisposalStopsFutureMutations(): Promise<void> {
  await withFixture(async ({ panel, text, writes, dispose }) => {
    panel.setAttribute("inactive", "true");
    await settle();
    assertEquals(writes(), 1, "observer delivers before disposal");
    dispose();
    await settle();
    text.setAttribute("hidden", "sentinel");
    await settle();
    const before = writes();
    panel.setAttribute("data-probe", "after-dispose");
    await settle();
    assertEquals(
      writes(),
      before,
      "disposed owner receives no future mutation",
    );
    assertEquals(
      text.getAttribute("hidden"),
      "sentinel",
      "disposed callback stays stopped",
    );
  });
}

async function testDisposalStopsDeliveredAndQueuedMutations(): Promise<void> {
  await withFixture(async ({ panel, text, writes, dispose }) => {
    panel.setAttribute("inactive", "true");
    await settle();
    assertEquals(writes(), 1, "observer is active before disposal");
    panel.setAttribute("data-probe", "queued-before-dispose");
    dispose();
    await settle();
    assertEquals(writes(), 1, "disposal discards the queued record");
    text.setAttribute("hidden", "sentinel");
    await settle();
    const before = writes();
    panel.setAttribute("data-probe", "after-dispose");
    await settle();
    assertEquals(
      writes(),
      before,
      "disposed owner receives no future mutation",
    );
    assertEquals(
      text.getAttribute("hidden"),
      "sentinel",
      "disposed callback stays stopped",
    );
  });
}

async function testMissingNodesRemainSafe(): Promise<void> {
  for (
    const missing of [
      ["nora-statusbar"],
      ["appcontent"],
      ["statuspanel-label"],
      ["statuspanel", "status-text"],
      ["statuspanel"],
      ["status-text"],
    ]
  ) {
    await withFixture(async ({ setEnabled, panel }) => {
      setEnabled(false);
      await settle();
      setEnabled(true);
      panel.setAttribute("inactive", "true");
      await settle();
    }, missing);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("observer-lifecycle.test.ts", [
    {
      name: "ON/OFF/ON releases prior observer without accumulation",
      fn: testToggleReleasesPreviousObserver,
    },
    {
      name: "OFF discards queued native observer delivery",
      fn: testOffDiscardsQueuedMutation,
    },
    {
      name: "reenabling keeps exactly one native observer callback",
      fn: testReenableDoesNotAccumulateObservers,
    },
    {
      name: "owner disposal stops future native mutations",
      fn: testDisposalStopsFutureMutations,
    },
    {
      name: "owner disposal stops queued and future native mutations",
      fn: testDisposalStopsDeliveredAndQueuedMutations,
    },
    {
      name: "missing status nodes remain safe across toggle and disposal",
      fn: testMissingNodesRemainSafe,
    },
  ]);
}
