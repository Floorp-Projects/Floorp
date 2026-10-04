// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { signal } from "@preact/signals";
import { createRef, Fragment, h } from "preact";
import { useLayoutEffect } from "preact/hooks";
import { act } from "preact/test-utils";
import { render, safeRender } from "../index.ts";
import { createRoot } from "../lifetime.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../browser-features/chrome/test/utils/test_harness.ts";

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const HTML_NS = "http://www.w3.org/1999/xhtml";

function host(): HTMLElement {
  const element = document.createElementNS(HTML_NS, "div");
  document.documentElement.appendChild(element);
  return element;
}

async function testNodeIdentityEventsAndRefs(): Promise<void> {
  const container = host();
  const label = signal("first");
  const itemRef = createRef<Element>();
  const callbackRefs: (Element | null)[] = [];
  const stableRef = (node: Element | null) => callbackRefs.push(node);
  let commands = 0;
  let popups = 0;
  let customEvents = 0;
  let dispose = () => {};
  try {
    await act(() => {
      dispose = safeRender(() =>
        h(
          "xul:menupopup",
          {
            ref: stableRef,
            onPopupShowing: () => popups++,
            onViewShowing: () => customEvents++,
          },
          h("xul:menuitem", {
            ref: itemRef,
            label: label.value,
            onCommand: () => commands++,
          }),
        ), container);
    });
    const popup = container.firstElementChild;
    assert(popup, "popup exists");
    const item = popup.firstElementChild;
    assert(item, "menu item exists");
    assertEquals(popup.namespaceURI, XUL_NS, "popup is native XUL");
    assertEquals(item.namespaceURI, XUL_NS, "nested item is native XUL");
    assertEquals(item.localName, "menuitem", "prefix is removed at creation");
    assertEquals(
      itemRef.current,
      item,
      "object ref is the attached native node",
    );
    assertEquals(
      callbackRefs[0],
      popup,
      "callback ref is the attached native node",
    );
    item.dispatchEvent(new Event("command"));
    popup.dispatchEvent(new Event("popupshowing"));
    popup.dispatchEvent(new Event("ViewShowing"));
    assertEquals(commands, 1, "native command listener survives creation");
    assertEquals(popups, 1, "popup event casing is normalized");
    assertEquals(customEvents, 1, "custom event casing is preserved");

    await act(() => {
      label.value = "second";
    });
    assertEquals(
      popup.firstElementChild,
      item,
      "signal updates preserve node identity",
    );
    assertEquals(
      item.getAttribute("label"),
      "second",
      "signal updates attached XUL",
    );
    item.dispatchEvent(new Event("command"));
    assertEquals(
      commands,
      2,
      "updated event handler stays on the attached node",
    );
    assertEquals(
      callbackRefs.length,
      1,
      "stable callback refs are not remounted on update",
    );
    await act(dispose);
    assertEquals(itemRef.current, null, "object ref clears on unmount");
    assertEquals(callbackRefs[1], null, "callback ref clears on unmount");
    assertEquals(
      container.childNodes.length,
      0,
      "unmount removes XUL and anchor",
    );
  } finally {
    dispose();
    container.remove();
  }
}

async function testIndependentRootsAndKeyedChildren(): Promise<void> {
  const container = host();
  const existing = document.createElementNS(HTML_NS, "span");
  const marker = document.createElementNS(HTML_NS, "span");
  existing.id = "native-before";
  marker.id = "native-after";
  container.append(existing, marker);
  const keys = signal(["a", "b"]);
  let disposeFirst = () => {};
  let disposeSecond = () => {};
  const ids = () => Array.from(container.children).map((el) => el.id).join(",");
  try {
    await act(() => {
      disposeFirst = safeRender(
        () => keys.value.map((key) => h("xul:menuitem", { key, id: key })),
        container,
        { marker },
      );
      disposeSecond = safeRender(
        h("xul:menuitem", { id: "independent" }),
        container,
      );
    });
    const a = container.querySelector("#a");
    const b = container.querySelector("#b");
    assertEquals(
      ids(),
      "native-before,a,b,native-after,independent",
      "insertion preserves native siblings",
    );
    assertEquals(
      a?.parentNode,
      container,
      "no HTML wrapper between menu and item",
    );
    await act(() => {
      keys.value = ["b", "a", "c"];
    });
    assertEquals(
      ids(),
      "native-before,b,a,c,native-after,independent",
      "keyed reorder stays inside root boundary",
    );
    assertEquals(
      container.querySelector("#a"),
      a,
      "keyed first node is reused",
    );
    assertEquals(
      container.querySelector("#b"),
      b,
      "keyed second node is reused",
    );
    await act(() => {
      keys.value = [];
    });
    assertEquals(
      ids(),
      "native-before,native-after,independent",
      "empty root preserves all foreign children",
    );
    await act(() => {
      keys.value = ["d", "e"];
    });
    assertEquals(
      ids(),
      "native-before,d,e,native-after,independent",
      "empty root can insert at original marker",
    );
    await act(disposeFirst);
    disposeFirst();
    assertEquals(
      ids(),
      "native-before,native-after,independent",
      "idempotent disposal affects only owned children",
    );
    await act(disposeSecond);
    assertEquals(
      ids(),
      "native-before,native-after",
      "second root has an independent lifetime",
    );
    assertEquals(
      container.childNodes.length,
      2,
      "all root markers are removed",
    );
  } finally {
    disposeFirst();
    disposeSecond();
    container.remove();
  }
}

async function testMixedNamespacesAndLifetime(): Promise<void> {
  const container = host();
  const text = signal("before");
  let mounts = 0;
  let cleanups = 0;
  let dispose = () => {};
  function Mixed() {
    useLayoutEffect(() => {
      mounts++;
      return () => cleanups++;
    }, []);
    return h(
      Fragment,
      {},
      h("xul:hbox", {}, h("span", {}, text.value)),
      h("svg", {}, h("circle", { r: 2 })),
    );
  }
  try {
    await act(() => {
      createRoot((cleanup) => {
        dispose = cleanup;
        render(Mixed, container);
      });
    });
    assertEquals(
      container.firstElementChild?.namespaceURI,
      XUL_NS,
      "XUL sibling namespace",
    );
    assertEquals(
      container.querySelector("span")?.namespaceURI,
      HTML_NS,
      "HTML inside XUL keeps JSX HTML namespace",
    );
    assertEquals(
      container.querySelector("circle")?.namespaceURI,
      "http://www.w3.org/2000/svg",
      "SVG remains supported",
    );
    assertEquals(mounts, 1, "component layout effect mounted once");
    await act(() => {
      text.value = "after";
    });
    assertEquals(
      container.querySelector("span")?.textContent,
      "after",
      "factory participates in signals rendering",
    );
    await act(dispose);
    dispose();
    assertEquals(cleanups, 1, "root disposal unmounts hook effects once");
    assertEquals(
      container.childNodes.length,
      0,
      "root disposal owns its render",
    );
    const native = document.createElementNS(XUL_NS, "label");
    assertEquals(
      native.namespaceURI,
      XUL_NS,
      "native unprefixed XUL factory calls are unchanged",
    );
  } finally {
    dispose();
    container.remove();
  }
}

function testNativeKeyAttribute(): void {
  const container = host();
  const dispose = safeRender(
    h("xul:key", { key: "R", modifiers: "accel,shift" }),
    container,
  );
  try {
    assertEquals(
      container.firstElementChild?.getAttribute("key"),
      "R",
      "reserved VNode key is also materialized as a native shortcut attribute",
    );
  } finally {
    dispose();
    container.remove();
  }
}

async function testNestedComponentNamespaces(): Promise<void> {
  const container = host();
  const expanded = signal(false);
  function Child() {
    return expanded.value
      ? h("input", { value: "edited" })
      : h("span", {}, "text");
  }
  const dispose = safeRender(h("xul:hbox", {}, h(Child, {})), container);
  try {
    await act(() => {
      expanded.value = true;
    });
    assertEquals(
      container.querySelector("input")?.namespaceURI,
      HTML_NS,
      "independently rerendered component under XUL creates HTML controls",
    );
  } finally {
    dispose();
    container.remove();
  }
}

async function testDirectSignalBindings(): Promise<void> {
  const container = host();
  const text = signal("before");
  const dispose = safeRender(
    h("xul:label", { "data-value": text }, text),
    container,
  );
  try {
    const label = container.firstElementChild;
    assert(label, "signal-bound label exists");
    assertEquals(
      label.getAttribute("data-value"),
      "before",
      "direct signal prop starts with current value",
    );
    await act(() => {
      text.value = "after";
    });
    assertEquals(
      label.getAttribute("data-value"),
      "after",
      "direct signal prop updates attached native element",
    );
    assertEquals(
      label.textContent,
      "after",
      "direct signal child updates attached text",
    );
    dispose();
    await act(() => {
      text.value = "disposed";
    });
    assertEquals(
      label.getAttribute("data-value"),
      "after",
      "unmount removes direct signal subscriptions",
    );
  } finally {
    dispose();
    container.remove();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("preact-xul/renderer.test.ts", [
    {
      name: "direct signal props and children update and unsubscribe",
      fn: testDirectSignalBindings,
    },
    {
      name: "nested component rerenders preserve HTML namespaces below XUL",
      fn: testNestedComponentNamespaces,
    },
    {
      name: "XUL keyboard shortcuts retain the reserved key attribute",
      fn: testNativeKeyAttribute,
    },
    {
      name: "native nodes retain identity, events, refs and signal updates",
      fn: testNodeIdentityEventsAndRefs,
    },
    {
      name:
        "independent roots preserve native children through keyed reconciliation",
      fn: testIndependentRootsAndKeyedChildren,
    },
    {
      name: "mixed namespaces and scoped render cleanup",
      fn: testMixedNamespacesAndLifetime,
    },
  ]);
}
