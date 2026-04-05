// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  deepQuerySelector,
  deepQuerySelectorAll,
  isElementVisible,
  unwrapDocument,
  unwrapElement,
  unwrapWindow,
} from "./utils.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

function requireDocument(): Document {
  if (!document) {
    throw new Error("document is unavailable in this test context");
  }
  return document;
}

function requireBody(doc: Document): HTMLElement {
  if (!doc.body) {
    throw new Error("document.body is unavailable in this test context");
  }
  return doc.body as HTMLElement;
}

function withFixture(run: (root: HTMLDivElement) => void): void {
  const doc = requireDocument();
  const root = doc.createElement("div");
  root.setAttribute("data-test-root", "webscraper-utils");
  requireBody(doc).appendChild(root);
  try {
    run(root);
  } finally {
    root.remove();
  }
}

function buildLineRect(width: number, height: number): DOMRect {
  return {
    x: 0,
    y: 0,
    top: 0,
    left: 0,
    right: width,
    bottom: height,
    width,
    height,
    toJSON(): string {
      return "";
    },
  } as DOMRect;
}

function buildNestedShadowChain(
  root: HTMLDivElement,
  depth: number,
): ShadowRoot {
  const doc = requireDocument();
  let currentRoot: ShadowRoot | HTMLDivElement = root;
  for (let i = 0; i < depth; i++) {
    const host = doc.createElement("div");
    host.className = `shadow-host-${i}`;
    currentRoot.appendChild(host);
    currentRoot = host.attachShadow({ mode: "open" });
  }
  return currentRoot as ShadowRoot;
}

function testDeepQuerySelectorAcrossShadowDom(): void {
  withFixture((root) => {
    const doc = requireDocument();
    const lightTarget = doc.createElement("span");
    lightTarget.className = "probe";
    lightTarget.textContent = "light";
    root.appendChild(lightTarget);

    const firstHost = doc.createElement("div");
    root.appendChild(firstHost);
    const firstShadow = firstHost.attachShadow({ mode: "open" });

    const secondHost = doc.createElement("div");
    firstShadow.appendChild(secondHost);
    const secondShadow = secondHost.attachShadow({ mode: "open" });

    const deepTarget = doc.createElement("span");
    deepTarget.className = "probe";
    deepTarget.id = "shadow-target";
    deepTarget.textContent = "shadow";
    secondShadow.appendChild(deepTarget);

    const found = deepQuerySelector(root, "#shadow-target");
    assertEquals(
      found,
      deepTarget,
      "deepQuerySelector should pierce nested open shadow roots",
    );

    const all = deepQuerySelectorAll(root, ".probe");
    assertEquals(
      all.length,
      2,
      "deepQuerySelectorAll should return light DOM and shadow DOM matches",
    );
    assert(
      all.includes(lightTarget),
      "querySelectorAll result should include light DOM target",
    );
    assert(
      all.includes(deepTarget),
      "querySelectorAll result should include deep shadow target",
    );
  });
}

function testDeepQuerySelectorDepthLimit(): void {
  withFixture((root) => {
    const doc = requireDocument();
    const reachableRoot = buildNestedShadowChain(root, 4);
    const reachable = doc.createElement("button");
    reachable.id = "reachable-depth-target";
    reachableRoot.appendChild(reachable);

    const foundReachable = deepQuerySelector(root, "#reachable-depth-target");
    assertEquals(
      foundReachable,
      reachable,
      "depth below limit should still be discoverable",
    );

    const tooDeepRoot = buildNestedShadowChain(root, 6);
    const tooDeep = doc.createElement("button");
    tooDeep.id = "too-deep-target";
    tooDeepRoot.appendChild(tooDeep);

    const foundTooDeep = deepQuerySelector(root, "#too-deep-target");
    assertEquals(
      foundTooDeep,
      null,
      "elements beyond MAX_SHADOW_DEPTH should not be discovered",
    );
  });
}

function testXrayUnwrapHelpers(): void {
  const doc = requireDocument();
  const rawWindow = window;
  const wrappedWindow = { wrappedJSObject: rawWindow } as unknown as Window &
    typeof globalThis;
  assertEquals(
    unwrapWindow(wrappedWindow),
    rawWindow,
    "unwrapWindow should prefer wrappedJSObject",
  );
  assertEquals(
    unwrapWindow(null),
    null,
    "unwrapWindow should return null when input is null",
  );

  const rawElement = doc.createElement("div");
  const wrappedElement = Object.assign(doc.createElement("div"), {
    wrappedJSObject: rawElement,
  });
  assertEquals(
    unwrapElement(wrappedElement),
    rawElement,
    "unwrapElement should return raw wrapped object",
  );

  const wrappedDocument = doc as Document &
    Partial<{ wrappedJSObject: Document }>;
  wrappedDocument.wrappedJSObject = doc;
  assertEquals(
    unwrapDocument(wrappedDocument),
    doc,
    "unwrapDocument should return wrapped document",
  );
}

function testIsElementVisible(): void {
  withFixture((root) => {
    const doc = requireDocument();
    const element = doc.createElement("div");
    root.appendChild(element);
    const runtimeWindow = window as unknown as Window & typeof globalThis;

    const setRect = (width: number, height: number): void => {
      Object.defineProperty(element, "getBoundingClientRect", {
        configurable: true,
        value: () => buildLineRect(width, height),
      });
    };

    setRect(100, 32);
    element.style.setProperty("display", "block");
    element.style.setProperty("visibility", "visible");
    element.style.setProperty("opacity", "1");
    assertEquals(
      isElementVisible(element, runtimeWindow),
      true,
      "visible element should return true",
    );

    element.style.setProperty("display", "none");
    assertEquals(
      isElementVisible(element, runtimeWindow),
      false,
      "display:none should be treated as invisible",
    );

    element.style.setProperty("display", "block");
    element.style.setProperty("visibility", "hidden");
    assertEquals(
      isElementVisible(element, runtimeWindow),
      false,
      "visibility:hidden should be treated as invisible",
    );

    element.style.setProperty("visibility", "visible");
    element.style.setProperty("opacity", "0");
    assertEquals(
      isElementVisible(element, runtimeWindow),
      false,
      "opacity:0 should be treated as invisible",
    );

    element.style.setProperty("opacity", "1");
    setRect(0, 20);
    assertEquals(
      isElementVisible(element, runtimeWindow),
      false,
      "zero-width element should be treated as invisible",
    );

    const throwingWindow = {
      getComputedStyle(): CSSStyleDeclaration {
        throw new Error("boom");
      },
    } as unknown as Window & typeof globalThis;
    assertEquals(
      isElementVisible(element, throwingWindow),
      false,
      "style access errors should be handled safely",
    );
  });
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "deep query selector across shadow DOM",
      fn: testDeepQuerySelectorAcrossShadowDom,
    },
    {
      name: "deep query selector depth limit",
      fn: testDeepQuerySelectorDepthLimit,
    },
    { name: "xray unwrap helpers", fn: testXrayUnwrapHelpers },
    { name: "is element visible", fn: testIsElementVisible },
  ];
  await runTests("utils.test.ts", tests);
}
