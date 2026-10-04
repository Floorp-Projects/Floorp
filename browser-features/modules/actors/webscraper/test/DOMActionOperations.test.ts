// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert as browserAssert,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import { DOMActionOperations } from "../DOMActionOperations.ts";
import type { DOMOpsDeps } from "../DOMDeps.ts";
import { HighlightManager } from "../HighlightManager.ts";
import type {
  HighlightOptionsInput,
  MouseSynthesisData,
  MouseSynthesisOptions,
  PrivilegedMouseWindow,
} from "../types.ts";

function assert(
  condition: unknown,
  message = "condition failed",
): asserts condition {
  browserAssert(condition, message);
}

function assertEquals(actual: unknown, expected: unknown): void {
  assert(
    JSON.stringify(actual) === JSON.stringify(expected),
    `expected ${JSON.stringify(expected)}, actual ${JSON.stringify(actual)}`,
  );
}

interface Delivery {
  type: string;
  x: number;
  y: number;
  data?: MouseSynthesisData;
  options?: MouseSynthesisOptions;
}

function fixture() {
  const state = {
    now: 0,
    nodes: [] as Element[],
    hit: null as Element | null,
    rect: { x: 30, y: 40, width: 100, height: 40 },
    connected: true,
    disabled: false,
    ariaDisabled: false,
    visible: true,
    deliveries: [] as Delivery[],
    waits: [] as number[],
    scrolls: 0,
    highlights: 0,
    legacyCalls: 0,
    onWait: null as (() => void) | null,
    onDelivery: null as ((type: string) => void) | null,
  };
  const doc = {
    querySelectorAll: () => state.nodes,
    elementFromPoint: () => state.hit,
  } as unknown as Document;
  function element(): Element {
    const el = {
      ownerDocument: doc,
      get isConnected() {
        return state.connected;
      },
      tagName: "BUTTON",
      textContent: "Activate",
      getBoundingClientRect: () =>
        new DOMRect(
          state.rect.x,
          state.rect.y,
          state.rect.width,
          state.rect.height,
        ),
      matches: () => state.disabled,
      closest: (): Element | null =>
        state.ariaDisabled ? el as unknown as Element : null,
      contains: (node: Node | null): boolean => node === el as unknown as Node,
      getRootNode: () => doc,
      scrollIntoView: () => {
        state.scrolls++;
      },
      click: () => {
        state.legacyCalls++;
      },
    };
    return el as unknown as Element;
  }
  const target = element();
  state.nodes = [target];
  state.hit = target;
  const win = {
    performance: { now: () => state.now },
    getComputedStyle: () => ({
      getPropertyValue: (property: string) =>
        property === "display" && !state.visible ? "none" : "",
    }),
    synthesizeMouseEvent: (
      type: string,
      x: number,
      y: number,
      data?: MouseSynthesisData,
      options?: MouseSynthesisOptions,
    ) => {
      state.deliveries.push({ type, x, y, data, options });
      state.onDelivery?.(type);
      return false;
    },
  } as unknown as PrivilegedMouseWindow;
  const deps = {
    getContentWindow: () => win,
    getDocument: () => doc,
    translationHelper: {
      truncate: (text: string, length: number) => text.slice(0, length),
      translate: () => Promise.resolve("Activate"),
    },
    highlightManager: {
      withControlOverlaySuspended: <T>(callback: () => T): T => callback(),
      getHighlightOptions: () => ({ action: "Click" }),
      applyHighlight: () => {
        state.highlights++;
        return Promise.resolve();
      },
    },
    eventDispatcher: {
      dispatchPointerClickSequence: () => {
        state.legacyCalls++;
        return true;
      },
      focusElementSoft: () => {
        state.legacyCalls++;
      },
      scrollIntoViewIfNeeded: () => {
        state.legacyCalls++;
      },
    },
  } as unknown as DOMOpsDeps;
  const ops = new DOMActionOperations(deps);
  (ops as unknown as { delay: (ms: number) => Promise<void> }).delay = (
    ms,
  ) => {
    state.waits.push(ms);
    state.now += ms;
    state.onWait?.();
    return Promise.resolve();
  };
  return { state, ops, win, doc, target, element, deps };
}

async function testSingleDeliveryIgnoresPreventDefaultReturn(): Promise<void> {
  const f = fixture();
  assertEquals(
    await f.ops.clickElement("#target", { timeout: 200, stabilityTimeout: 50 }),
    true,
  );
  assertEquals(f.state.now, 50);
  assertEquals(f.state.deliveries.map((d) => d.type), [
    "mousemove",
    "mousedown",
    "mouseup",
  ]);
  assertEquals(f.state.deliveries.map((d) => d.data?.clickCount), [0, 1, 1]);
  assertEquals(f.state.deliveries.map((d) => d.data?.buttons), [0, 1, 0]);
  assert(
    f.state.deliveries.every((d) =>
      d.x === 80 && d.y === 60 && d.options?.toWindow === true
    ),
  );
  assertEquals(f.state.legacyCalls, 0);
}

async function testDeferredHighlightPreservesPageInteraction(): Promise<void> {
  const f = fixture();
  const translation = Promise.withResolvers<string>();
  const highlighted = Promise.withResolvers<void>();
  const appliedOptions: HighlightOptionsInput[] = [];
  f.deps.translationHelper.translate = () => translation.promise;
  // Use the real Click preset and override merging, whose defaults focus the
  // target and scroll smoothly unless the click operation opts out.
  f.deps.highlightManager.getHighlightOptions =
    HighlightManager.prototype.getHighlightOptions;
  f.deps.highlightManager.applyHighlight = (_target, options = {}) => {
    appliedOptions.push(options);
    f.state.highlights++;
    highlighted.resolve();
    return Promise.resolve(true);
  };

  const result = await f.ops.clickElementWithResult("#target", {
    timeout: 200,
    stabilityTimeout: 0,
  });
  assertEquals(result.status, "dispatched");
  assertEquals(f.state.highlights, 0);
  assertEquals(f.state.deliveries.map((delivery) => delivery.type), [
    "mousemove",
    "mousedown",
    "mouseup",
  ]);
  const completedDeliveries = [...f.state.deliveries];
  const completedScrolls = f.state.scrolls;

  // Translation finishes after the API returns, when page handlers or the
  // caller may already have moved focus and scrolled to their next target.
  translation.resolve("Activate");
  await highlighted.promise;
  assertEquals(f.state.highlights, 1);
  assertEquals(appliedOptions[0].action, "Click");
  assertEquals(appliedOptions[0].focus, false);
  assertEquals(appliedOptions[0].scrollBehavior, "none");
  assertEquals(f.state.deliveries, completedDeliveries);
  assertEquals(f.state.scrolls, completedScrolls);
  assertEquals(f.state.legacyCalls, 0);
}

async function testAbsentTargetAppearsAfterThreeOldRetries(): Promise<void> {
  const f = fixture();
  f.state.nodes = [];
  f.state.onWait = () => {
    if (f.state.now >= 450) f.state.nodes = [f.target];
  };
  assertEquals(
    await f.ops.clickElement("#target", {
      timeout: 1000,
      stabilityTimeout: 50,
    }),
    true,
  );
  assertEquals(f.state.now, 500);
}

async function testAmbiguityRefusesWithoutDelivery(): Promise<void> {
  for (const force of [false, true]) {
    const f = fixture();
    f.state.nodes.push(f.element());
    assertEquals(
      await f.ops.clickElement(".ambiguous", { force, timeout: 200 }),
      false,
    );
    assertEquals(f.state.deliveries.length, 0);
    assertEquals(f.state.waits.length, 0);
  }
}

async function testDisabledHiddenAndCoveredTargetsRefuse(): Promise<void> {
  const patches = [
    (f: ReturnType<typeof fixture>) => {
      f.state.disabled = true;
    },
    (f: ReturnType<typeof fixture>) => {
      f.state.ariaDisabled = true;
    },
    (f: ReturnType<typeof fixture>) => {
      f.state.visible = false;
    },
    (f: ReturnType<typeof fixture>) => {
      f.state.hit = f.element();
    },
    (f: ReturnType<typeof fixture>) => {
      f.state.connected = false;
    },
  ];
  for (const patch of patches) {
    const f = fixture();
    patch(f);
    assertEquals(
      await f.ops.clickElement("#target", {
        timeout: 63,
        stabilityTimeout: 50,
      }),
      false,
    );
    assertEquals(f.state.now, 63);
    assertEquals(f.state.waits, [25, 25, 13]);
    assertEquals(f.state.deliveries.length, 0);
  }
}

async function testTemporaryBlockerWaits(): Promise<void> {
  for (const kind of ["disabled", "overlay"]) {
    const f = fixture();
    if (kind === "disabled") f.state.disabled = true;
    else f.state.hit = f.element();
    f.state.onWait = () => {
      if (f.state.now >= 75) {
        f.state.disabled = false;
        f.state.hit = f.target;
      }
    };
    assertEquals(
      await f.ops.clickElement("#target", {
        timeout: 250,
        stabilityTimeout: 50,
      }),
      true,
    );
    assertEquals(f.state.now, 125);
  }
}

async function testMovementResetsStability(): Promise<void> {
  const f = fixture();
  f.state.onWait = () => {
    if (f.state.now <= 100) f.state.rect.x += 5;
  };
  assertEquals(
    await f.ops.clickElement("#target", { timeout: 300, stabilityTimeout: 50 }),
    true,
  );
  assertEquals(f.state.now, 150);
}

async function testReplacementRestartsStability(): Promise<void> {
  const f = fixture();
  const replacement = f.element();
  f.state.onWait = () => {
    if (f.state.now === 25) {
      f.state.nodes = [replacement];
      f.state.hit = replacement;
    }
  };
  assertEquals(
    await f.ops.clickElement("#target", { timeout: 200, stabilityTimeout: 50 }),
    true,
  );
  assertEquals(f.state.now, 75);
}

async function testPointerMoveInvalidationStopsBeforePress(): Promise<void> {
  const patches = [
    (f: ReturnType<typeof fixture>) => {
      f.state.nodes = [f.element()];
    },
    (f: ReturnType<typeof fixture>) => {
      f.state.nodes.push(f.element());
    },
    (f: ReturnType<typeof fixture>) => {
      f.state.hit = f.element();
    },
    (f: ReturnType<typeof fixture>) => {
      f.state.disabled = true;
    },
    (f: ReturnType<typeof fixture>) => {
      f.state.rect.x += 5;
    },
    (f: ReturnType<typeof fixture>) => {
      f.state.connected = false;
    },
    (f: ReturnType<typeof fixture>) => {
      f.state.now = 200;
    },
  ];
  for (const patch of patches) {
    const f = fixture();
    f.state.onDelivery = (type) => {
      if (type === "mousemove") patch(f);
    };
    assertEquals(
      await f.ops.clickElement("#target", {
        timeout: 200,
        stabilityTimeout: 50,
      }),
      false,
    );
    assertEquals(f.state.deliveries.map((d) => d.type), ["mousemove"]);
    assertEquals(f.state.legacyCalls, 0);
  }
}

async function testMissingNativeCapabilityRefuses(): Promise<void> {
  const f = fixture();
  delete f.win.synthesizeMouseEvent;
  const result = await f.ops.clickElementWithResult("#target", {
    timeout: 200,
  });
  assertEquals(result.ok, false);
  assertEquals(result.status, "unsupported");
  assertEquals(result.phase, "prepare");
  assertEquals(result.inputStarted, false);
  assertEquals(result.activationStarted, false);
  assertEquals(f.state.waits.length, 0);
  assertEquals(f.state.legacyCalls, 0);
}

async function testDeadlineDuringFinalGuardStopsBeforePress(): Promise<void> {
  const f = fixture();
  let queries = 0;
  f.doc.querySelectorAll = (() => {
    // Candidate lookup, pre-move guard, then the final pre-press guard.
    if (++queries === 3) f.state.now = 500;
    return f.state.nodes;
  }) as unknown as typeof f.doc.querySelectorAll;
  const result = await f.ops.clickElementWithResult("#target", {
    timeout: 200,
    stabilityTimeout: 0,
  });
  assertEquals(result.ok, false);
  assertEquals(result.activationStarted, false);
  assertEquals(f.state.deliveries.map((d) => d.type), ["mousemove"]);
}

async function testPartialNativeFailureNeverRetries(): Promise<void> {
  for (const failure of ["mousemove", "mousedown", "mouseup"]) {
    const f = fixture();
    f.state.onDelivery = (type) => {
      if (type === failure) throw new Error("Injected native failure");
    };
    const result = await f.ops.clickElementWithResult("#target", {
      timeout: 200,
      stabilityTimeout: 0,
    });
    assertEquals(result.ok, false);
    assertEquals(result.status, "unknown");
    assertEquals(result.phase, failure);
    assertEquals(result.inputStarted, true);
    assertEquals(result.activationStarted, failure !== "mousemove");
    assertEquals(
      f.state.deliveries.filter((d) => d.type === failure).length,
      1,
    );
    assertEquals(f.state.legacyCalls, 0);
  }
}

async function testPerCallResultsAndCommitRefusal(): Promise<void> {
  const f = fixture();
  f.state.nodes.push(f.element());
  const refused = await f.ops.clickElementWithResult(".ambiguous", {
    timeout: 200,
  });
  assertEquals(refused.status, "refused");
  assertEquals(refused.reason, "ambiguous-selector");
  assertEquals(refused.phase, "wait");
  assertEquals(refused.inputStarted, false);
  assertEquals(refused.activationStarted, false);
  f.state.nodes = [f.target];
  const success = await f.ops.clickElementWithResult("#target", {
    timeout: 200,
    stabilityTimeout: 0,
  });
  assertEquals(success.ok, true);
  assertEquals(success.status, "dispatched");
  assertEquals(success.phase, "complete");
  assertEquals(success.inputStarted, true);
  assertEquals(success.activationStarted, true);
  assertEquals(refused.status, "refused");
  assertEquals(refused.inputStarted, false);
  const changed = fixture();
  changed.state.onDelivery = (type) => {
    if (type === "mousemove") changed.state.disabled = true;
  };
  const commit = await changed.ops.clickElementWithResult("#target", {
    timeout: 200,
    stabilityTimeout: 0,
  });
  assertEquals(commit.status, "refused");
  assertEquals(commit.reason, "eligibility-changed-before-mousedown");
  assertEquals(commit.phase, "mousemove");
  assertEquals(commit.inputStarted, true);
  assertEquals(commit.activationStarted, false);
  assertEquals(changed.state.deliveries.map((d) => d.type), ["mousemove"]);
}

async function testPartialDoubleClickIsNotSafeRefusal(): Promise<void> {
  const f = fixture();
  f.state.onDelivery = (type) => {
    if (type === "mouseup") f.state.disabled = true;
  };
  const result = await f.ops.clickElementWithResult("#target", {
    timeout: 200,
    stabilityTimeout: 0,
    clickCount: 2,
  });
  assertEquals(result.ok, false);
  assertEquals(result.status, "unknown");
  assertEquals(result.reason, "eligibility-changed-after-activation");
  assertEquals(result.phase, "mouseup");
  assertEquals(result.activationStarted, true);
  assertEquals(f.state.deliveries.map((d) => d.type), [
    "mousemove",
    "mousedown",
    "mouseup",
  ]);
}

async function testExplicitForceAndMultipleClickSemantics(): Promise<void> {
  const f = fixture();
  f.state.disabled = true;
  f.state.visible = false;
  f.state.hit = null;
  assertEquals(
    await f.ops.clickElement("#target", { force: true, timeout: 200 }),
    true,
  );
  assertEquals(f.state.now, 0);
  for (const button of ["left", "middle", "right"] as const) {
    const d = fixture();
    assertEquals(
      await d.ops.clickElement("#target", {
        button,
        clickCount: 2,
        timeout: 200,
        stabilityTimeout: 0,
      }),
      true,
    );
    assertEquals(d.state.deliveries.map((e) => e.type), [
      "mousemove",
      "mousedown",
      "mouseup",
      "mousedown",
      "mouseup",
    ]);
    assertEquals(d.state.deliveries.map((e) => e.data?.clickCount), [
      0,
      1,
      1,
      2,
      2,
    ]);
    assertEquals(
      d.state.deliveries[1].data?.button,
      { left: 0, middle: 1, right: 2 }[button],
    );
    assertEquals(
      d.state.deliveries[1].data?.buttons,
      { left: 1, middle: 4, right: 2 }[button],
    );
  }
}

async function testBudgetAndInvalidOptions(): Promise<void> {
  for (const timeout of [0, -1, NaN, Infinity]) {
    const f = fixture();
    assertEquals(await f.ops.clickElement("#target", { timeout }), false);
    assertEquals(f.state.deliveries.length, 0);
    assertEquals(f.state.waits.length, 0);
  }
  for (const stabilityTimeout of [-1, NaN, Infinity]) {
    const f = fixture();
    assertEquals(
      await f.ops.clickElement("#target", { stabilityTimeout }),
      false,
    );
  }
  for (const clickCount of [0, -1, 1.5, 3, NaN, Infinity]) {
    const f = fixture();
    assertEquals(await f.ops.clickElement("#target", { clickCount }), false);
    assertEquals(f.state.deliveries.length, 0);
  }
  const small = fixture();
  assertEquals(
    await small.ops.clickElement("#target", {
      timeout: 40,
      stabilityTimeout: 50,
    }),
    false,
  );
  assertEquals(small.state.waits, [25, 15]);
  const capped = fixture();
  capped.state.nodes = [];
  assertEquals(
    await capped.ops.clickElement("#target", { timeout: 100_000 }),
    false,
  );
  assertEquals(capped.state.now, 60_000);
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "single native sequence ignores preventDefault return",
      fn: testSingleDeliveryIgnoresPreventDefaultReturn,
    },
    {
      name: "deferred click highlight cannot focus scroll or deliver more input",
      fn: testDeferredHighlightPreservesPageInteraction,
    },
    {
      name: "late target is awaited beyond three attempts",
      fn: testAbsentTargetAppearsAfterThreeOldRetries,
    },
    {
      name: "ambiguous selector refuses including force",
      fn: testAmbiguityRefusesWithoutDelivery,
    },
    {
      name: "disabled hidden covered and detached targets refuse",
      fn: testDisabledHiddenAndCoveredTargetsRefuse,
    },
    {
      name: "temporary disabled or overlay waits",
      fn: testTemporaryBlockerWaits,
    },
    {
      name: "movement resets stability anchor",
      fn: testMovementResetsStability,
    },
    {
      name: "replacement restarts stability on new candidate",
      fn: testReplacementRestartsStability,
    },
    {
      name: "mousemove changes stop before mousedown",
      fn: testPointerMoveInvalidationStopsBeforePress,
    },
    {
      name: "missing native capability refuses without DOM fallback",
      fn: testMissingNativeCapabilityRefuses,
    },
    {
      name: "expiry during final target query stops before press",
      fn: testDeadlineDuringFinalGuardStopsBeforePress,
    },
    {
      name: "partial delivery failure never retries",
      fn: testPartialNativeFailureNeverRetries,
    },
    {
      name: "per call diagnosis distinguishes prepare and partial refusal",
      fn: testPerCallResultsAndCommitRefusal,
    },
    {
      name: "partial double click does not count as safe refusal",
      fn: testPartialDoubleClickIsNotSafeRefusal,
    },
    {
      name: "explicit force double and alternate buttons retain semantics",
      fn: testExplicitForceAndMultipleClickSemantics,
    },
    {
      name: "zero invalid clamped and insufficient budgets",
      fn: testBudgetAndInvalidOptions,
    },
  ];
  await runTests("DOMActionOperations.test.ts", tests);
}
