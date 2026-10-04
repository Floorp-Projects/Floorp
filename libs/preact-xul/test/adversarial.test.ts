// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { signal } from "@preact/signals";
import { Component, type ComponentChildren, createRef, h } from "preact";
import { createPortal } from "preact/compat";
import { useLayoutEffect } from "preact/hooks";
import { act } from "preact/test-utils";
import type { ViteHotContext } from "vite/types/hot.js";
import { safeRender } from "../index.ts";
import {
  addDisposer,
  createNodeDisposer,
  createRootHMR,
  disposeRoot,
} from "../lifetime.ts";
import {
  assert,
  assertEquals,
  assertThrows,
  runTests,
} from "../../../browser-features/chrome/test/utils/test_harness.ts";

const HTML = "http://www.w3.org/1999/xhtml";
const SVG = "http://www.w3.org/2000/svg";
const XUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

function host(namespace = HTML, name = "div"): Element {
  const node = document.createElementNS(namespace, name);
  document.documentElement.appendChild(node);
  return node;
}

async function testSVGContainer(): Promise<void> {
  const container = host(SVG, "svg");
  const mode = signal(false);
  const dispose = safeRender(
    () => mode.value ? h("rect", { width: 2 }) : h("circle", { r: 1 }),
    container,
  );
  try {
    assertEquals(
      container.firstElementChild?.namespaceURI,
      SVG,
      "initial child of an SVG render container is SVG",
    );
    await act(() => {
      mode.value = true;
    });
    assertEquals(
      container.firstElementChild?.namespaceURI,
      SVG,
      "replacement in an SVG render container remains SVG",
    );
  } finally {
    dispose();
    container.remove();
  }
  const foreignObject = host(SVG, "foreignObject");
  const disposeHTML = safeRender(h("input", {}), foreignObject);
  try {
    assertEquals(
      foreignObject.firstElementChild?.namespaceURI,
      HTML,
      "foreignObject render container starts an HTML subtree",
    );
  } finally {
    disposeHTML();
    foreignObject.remove();
  }
}

async function testDeepMixedNamespaces(): Promise<void> {
  const container = host();
  const mode = signal(false);
  function InsideXUL() {
    return h(
      "svg",
      {},
      h(
        "foreignObject",
        {},
        h("xul:hbox", {}, mode.value ? h("input", {}) : h("span", {})),
      ),
    );
  }
  const dispose = safeRender(h("xul:vbox", {}, h(InsideXUL, {})), container);
  try {
    assertEquals(
      container.querySelector("svg")?.namespaceURI,
      SVG,
      "SVG inside XUL",
    );
    assertEquals(
      container.querySelector("hbox")?.namespaceURI,
      XUL,
      "XUL inside SVG foreignObject",
    );
    assertEquals(
      container.querySelector("span")?.namespaceURI,
      HTML,
      "HTML inside nested XUL",
    );
    await act(() => {
      mode.value = true;
    });
    assertEquals(
      container.querySelector("input")?.namespaceURI,
      HTML,
      "nested replacement preserves HTML namespace",
    );
  } finally {
    dispose();
    container.remove();
  }
}

async function testCaptureAndCustomEvents(): Promise<void> {
  const container = host();
  const active = signal(true);
  const events: string[] = [];
  const dispose = safeRender(() =>
    h("xul:vbox", {
      onCommandCapture: active.value ? () => events.push("capture") : null,
      onCommand: () => events.push("bubble"),
      onViewShowingCapture: () => events.push("custom-capture"),
      onViewShowing: () => events.push("custom-bubble"),
    }, h("xul:button", { onCommand: () => events.push("target") })), container);
  try {
    const target = container.querySelector("button");
    assert(target, "event target exists");
    target.dispatchEvent(new Event("command", { bubbles: true }));
    assertEquals(
      events.join(","),
      "capture,target,bubble",
      "XUL command capture and bubble order",
    );
    events.length = 0;
    target.dispatchEvent(new Event("ViewShowing", { bubbles: true }));
    assertEquals(
      events.join(","),
      "custom-capture,custom-bubble",
      "custom event casing survives capture",
    );
    events.length = 0;
    await act(() => {
      active.value = false;
    });
    target.dispatchEvent(new Event("command", { bubbles: true }));
    assertEquals(
      events.join(","),
      "target,bubble",
      "removing capture leaves bubbling listeners intact",
    );
  } finally {
    dispose();
    container.remove();
  }
}

async function testPortalOwnershipAndRetargeting(): Promise<void> {
  const container = host();
  const first = host(XUL, "vbox");
  const second = host(XUL, "vbox");
  const native = document.createElementNS(XUL, "label");
  first.appendChild(native);
  const destination = signal(first);
  const label = signal("before");
  const ref = createRef<Element>();
  let commands = 0;
  const dispose = safeRender(() =>
    createPortal(
      h("xul:button", {
        ref,
        label: label.value,
        onCommand: () => commands++,
      }, h("input", { value: label.value })),
      destination.value,
    ), container);
  try {
    assertEquals(
      ref.current?.parentNode,
      first,
      "portal mounts beside existing browser children",
    );
    assertEquals(
      first.querySelector("input")?.namespaceURI,
      HTML,
      "portal to XUL preserves HTML controls",
    );
    await act(() => {
      label.value = "after";
    });
    assertEquals(
      ref.current?.getAttribute("label"),
      "after",
      "portal reacts to signals",
    );
    await act(() => {
      destination.value = second;
    });
    assertEquals(
      ref.current?.parentNode,
      second,
      "portal retargets to another native parent",
    );
    assertEquals(
      first.children.length,
      1,
      "old target preserves its native child",
    );
    ref.current?.dispatchEvent(new Event("command"));
    assertEquals(commands, 1, "portal listener survives retargeting");
    dispose();
    assertEquals(
      ref.current,
      null,
      "portal ref clears on parent-root disposal",
    );
    assertEquals(
      second.children.length,
      0,
      "portal elements unmount with the owner",
    );
    assertEquals(
      native.parentNode,
      first,
      "unmount preserves unrelated native children",
    );
  } finally {
    dispose();
    container.remove();
    first.remove();
    second.remove();
  }
}

async function testRefCleanupAndMovedContainer(): Promise<void> {
  const container = host();
  const destination = host();
  const marker = document.createElementNS(HTML, "span");
  container.appendChild(marker);
  const label = signal("before");
  let cleanups = 0;
  let mounts = 0;
  const ref = (node: Element | null) => {
    if (node) {
      mounts++;
      return () => cleanups++;
    }
  };
  const dispose = safeRender(
    () => h("xul:label", { ref, value: label.value }),
    container,
    marker,
  );
  try {
    destination.appendChild(container);
    marker.remove();
    await act(() => {
      label.value = "after";
    });
    assertEquals(
      container.firstElementChild?.getAttribute("value"),
      "after",
      "moving container and removing original marker preserves its root",
    );
    assertEquals(mounts, 1, "container move does not remount stable refs");
    dispose();
    dispose();
    assertEquals(cleanups, 1, "ref cleanup callback runs exactly once");
    assertEquals(
      container.childNodes.length,
      0,
      "unmount removes nodes and internal marker",
    );
  } finally {
    dispose();
    destination.remove();
    container.remove();
  }
}

class Boundary
  extends Component<{ children?: ComponentChildren }, { failed: boolean }> {
  override state = { failed: false };
  static override getDerivedStateFromError() {
    return { failed: true };
  }
  render() {
    return this.state.failed
      ? h("input", { "data-recovered": "yes" })
      : this.props.children;
  }
}

async function testRenderErrorsRestoreNamespaceState(): Promise<void> {
  const container = host(XUL, "vbox");
  const failure = signal(false);
  function Broken() {
    if (failure.value) throw new Error("expected renderer regression error");
    return h("span", {}, "ok");
  }
  const dispose = safeRender(h(Boundary, {}, h(Broken, {})), container);
  try {
    await act(() => {
      failure.value = true;
    });
    assertEquals(
      container.firstElementChild?.namespaceURI,
      HTML,
      "error-boundary fallback under XUL remains HTML",
    );
    assertEquals(
      container.firstElementChild?.getAttribute("data-recovered"),
      "yes",
      "boundary handles thrown render error",
    );
    assertEquals(
      document.createElementNS(XUL, "input").namespaceURI,
      XUL,
      "render error leaves browser-native namespace requests untouched",
    );
  } finally {
    dispose();
    container.remove();
  }
  const empty = host();
  try {
    assertThrows(() =>
      safeRender(() => {
        throw new Error("unhandled setup");
      }, empty), "unhandled initial render error propagates");
    assertEquals(
      empty.childNodes.length,
      0,
      "failed initial render leaves no root marker",
    );
    assertEquals(
      document.createElementNS(XUL, "label").namespaceURI,
      XUL,
      "unhandled error clears namespace state",
    );
  } finally {
    empty.remove();
  }
}

async function testNodeAdoption(): Promise<void> {
  const node = host();
  const destination = document.implementation.createHTMLDocument(
    "node lifetime",
  );
  let cleanups = 0;
  const dispose = createNodeDisposer(node, () => cleanups++);
  try {
    destination.body.appendChild(node);
    await Promise.resolve();
    await Promise.resolve();
    assertEquals(cleanups, 0, "document adoption preserves a connected node");
    node.remove();
    await Promise.resolve();
    await Promise.resolve();
    assertEquals(
      cleanups,
      1,
      "removal after adoption still releases resources",
    );
  } finally {
    dispose();
    node.remove();
  }
}

function testFailedHMRRegistration(): void {
  let attempts = 0;
  let registered: (() => void) | undefined;
  let cleanups = 0;
  const hot = {
    data: {},
    dispose(callback: () => void) {
      attempts++;
      if (attempts === 1) throw new Error("registration failed");
      registered = callback;
    },
  } as unknown as ViteHotContext;
  try {
    assertThrows(
      () => createRootHMR(() => {}, hot),
      "failed HMR registration propagates",
    );
    createRootHMR(() => addDisposer(() => cleanups++), hot);
    assertEquals(attempts, 2, "a later root retries failed HMR registration");
    registered?.();
    assertEquals(cleanups, 1, "recovered HMR callback disposes its root");
  } finally {
    disposeRoot(hot);
  }
}

async function testUnmountErrorsReleaseSiblings(): Promise<void> {
  const container = host();
  const source = signal(0);
  let renders = 0;
  let released = 0;
  const survivor = createRef<Element>();
  function BadEffect() {
    useLayoutEffect(() => () => {
      throw new Error("effect cleanup failure");
    }, []);
    return h("xul:label", {});
  }
  function GoodEffect() {
    useLayoutEffect(() => () => released++, []);
    renders++;
    return h("xul:label", { ref: survivor, value: source.value });
  }
  const dispose = safeRender([
    h(BadEffect, {}),
    h("xul:label", {
      ref: (node: Element | null) =>
        node
          ? () => {
            throw new Error("ref cleanup failure");
          }
          : undefined,
    }),
    h(GoodEffect, {}),
  ], container);
  try {
    assertThrows(dispose, "unmount still reports failing cleanup");
    assertEquals(
      released,
      1,
      "a failing cleanup must not skip sibling hook cleanup",
    );
    assertEquals(
      survivor.current,
      null,
      "sibling refs clear despite another cleanup failure",
    );
    assertEquals(
      container.childNodes.length,
      0,
      "failed unmount leaves no owned DOM or marker",
    );
    const previousRenders = renders;
    await act(() => {
      source.value++;
    });
    assertEquals(
      renders,
      previousRenders,
      "all signal subscriptions stop after failed cleanup",
    );
  } finally {
    dispose();
    container.remove();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("preact-xul/adversarial.test.ts", [
    {
      name: "SVG destination namespace survives replacement",
      fn: testSVGContainer,
    },
    {
      name: "deep XUL SVG foreignObject and HTML nesting",
      fn: testDeepMixedNamespaces,
    },
    {
      name: "XUL and case-sensitive custom event capture",
      fn: testCaptureAndCustomEvents,
    },
    {
      name: "portal ownership, retargeting and disposal",
      fn: testPortalOwnershipAndRetargeting,
    },
    {
      name: "ref cleanup after container and marker movement",
      fn: testRefCleanupAndMovedContainer,
    },
    {
      name: "render errors reset namespace state",
      fn: testRenderErrorsRestoreNamespaceState,
    },
    { name: "node lifecycle follows document adoption", fn: testNodeAdoption },
    {
      name: "HMR registration failure can recover",
      fn: testFailedHMRRegistration,
    },
    {
      name:
        "unmount errors still release sibling hooks, refs and subscriptions",
      fn: testUnmountErrorsReleaseSiblings,
    },
  ]);
}
