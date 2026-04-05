// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  FingerprintTextCache,
  findElementByFingerprint,
  findElementsByFingerprint,
  generateFingerprint,
  resolveElementLocator,
} from "./fingerprint.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

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
  root.setAttribute("data-test-root", "fingerprint-dom");
  requireBody(doc).appendChild(root);
  try {
    run(root);
  } finally {
    root.remove();
  }
}

function testFingerprintIgnoresVolatileAttributes(): void {
  withFixture((root) => {
    const doc = requireDocument();
    const target = doc.createElement("p");
    target.textContent = "Stable content for fingerprinting";
    target.id = "first-id";
    target.className = "first-class";
    target.setAttribute("data-session", "abc");
    root.appendChild(target);

    const first = generateFingerprint(target);

    target.id = "second-id";
    target.className = "second-class";
    target.setAttribute("style", "color: red;");
    target.setAttribute("data-session", "xyz");

    const second = generateFingerprint(target);
    assertEquals(
      second.short,
      first.short,
      "id/class/style/data-* changes should not alter short fingerprint",
    );
    assertEquals(
      second.full,
      first.full,
      "id/class/style/data-* changes should not alter full fingerprint",
    );
  });
}

function testFingerprintDeterminismWithNoisyChildren(): void {
  withFixture((root) => {
    const doc = requireDocument();
    const card = doc.createElement("article");

    const paragraph = doc.createElement("p");
    paragraph.textContent = "Visible text";
    const noscript = doc.createElement("noscript");
    noscript.textContent = "fallback text that should be ignored";
    const style = doc.createElement("style");
    style.textContent = ".x{color:red}";

    card.appendChild(paragraph);
    card.appendChild(noscript);
    card.appendChild(style);
    root.appendChild(card);

    const withNoiseFirst = generateFingerprint(card);
    const withNoiseSecond = generateFingerprint(card);
    assertEquals(
      withNoiseSecond.short,
      withNoiseFirst.short,
      "fingerprint should be deterministic on repeated calls",
    );
    assertEquals(
      withNoiseSecond.full,
      withNoiseFirst.full,
      "full fingerprint should be deterministic on repeated calls",
    );

    const noisyNodes = Array.from(
      card.querySelectorAll("noscript,style"),
    ) as Element[];
    for (const noisyNode of noisyNodes) {
      noisyNode.remove();
    }

    const withoutNoise = generateFingerprint(card);
    assertEquals(
      typeof withoutNoise.short,
      "string",
      "short fingerprint should remain available after DOM mutation",
    );
    assertEquals(
      typeof withoutNoise.full,
      "string",
      "full fingerprint should remain available after DOM mutation",
    );
    assertEquals(
      withoutNoise.short.length,
      8,
      "short fingerprint should keep expected length",
    );
    assertEquals(
      withoutNoise.full.length,
      16,
      "full fingerprint should keep expected length",
    );
  });
}

function testFingerprintTextCacheConsistency(): void {
  withFixture((root) => {
    const doc = requireDocument();
    const section = doc.createElement("section");
    const heading = doc.createElement("h1");
    heading.textContent = "Headline";

    const paragraph = doc.createElement("p");
    paragraph.appendChild(doc.createTextNode("Paragraph "));
    const strong = doc.createElement("strong");
    strong.textContent = "with nested text";
    paragraph.appendChild(strong);

    const listLike = doc.createElement("div");
    const leafA = doc.createElement("span");
    leafA.textContent = "Leaf A";
    const leafB = doc.createElement("span");
    leafB.textContent = "Leaf B";
    listLike.appendChild(leafA);
    listLike.appendChild(leafB);

    section.appendChild(heading);
    section.appendChild(paragraph);
    section.appendChild(listLike);
    root.appendChild(section);

    const cache = new FingerprintTextCache();
    cache.precompute(section);

    const elements = [section, paragraph, strong];
    for (const element of elements) {
      const noCache = generateFingerprint(element);
      const withCache = generateFingerprint(element, {}, cache);
      assertEquals(
        withCache.short,
        noCache.short,
        "text cache should not change short fingerprint output",
      );
      assertEquals(
        withCache.full,
        noCache.full,
        "text cache should not change full fingerprint output",
      );
    }
  });
}

function testFindAndResolveByFingerprint(): void {
  withFixture((root) => {
    const doc = requireDocument();
    const target = doc.createElement("button");
    target.id = "target-button";
    target.textContent = "Click me";

    const other = doc.createElement("span");
    other.textContent = "Other content";

    const overlay = doc.createElement("div");
    overlay.className = "nr-webscraper-overlay";
    overlay.textContent = "Transient overlay";

    root.appendChild(target);
    root.appendChild(other);
    root.appendChild(overlay);

    const targetFp = generateFingerprint(target);
    const overlayFp = generateFingerprint(overlay);

    const body = requireBody(doc);
    const shortFound = findElementByFingerprint(body, targetFp.short);
    assertEquals(
      shortFound,
      target,
      "short fingerprint should resolve matching element",
    );

    const fullFound = findElementByFingerprint(body, targetFp.full);
    assertEquals(
      fullFound,
      target,
      "full fingerprint should resolve matching element",
    );

    const allFound = findElementsByFingerprint(body, targetFp.full);
    assertEquals(
      allFound.length,
      1,
      "findElementsByFingerprint should return one match for unique fingerprint",
    );
    assertEquals(
      allFound[0],
      target,
      "findElementsByFingerprint should contain the target element",
    );

    const invalidLookup = findElementByFingerprint(body, "bad-fp");
    assertEquals(
      invalidLookup,
      null,
      "invalid fingerprint format should return null without traversal",
    );

    const invalidList = findElementsByFingerprint(body, "bad-fp");
    assertEquals(
      invalidList.length,
      0,
      "invalid fingerprint format should return empty match list",
    );

    const overlayFound = findElementByFingerprint(body, overlayFp.short);
    assertEquals(
      overlayFound,
      null,
      "highlight overlay nodes should be excluded from fingerprint lookup traversal",
    );

    const selectorPreferred = resolveElementLocator(doc, {
      selector: "#target-button",
      fingerprint: targetFp.short,
    });
    assertEquals(
      selectorPreferred,
      target,
      "CSS selector should take priority over fingerprint when both are provided",
    );

    const fingerprintFallback = resolveElementLocator(doc, {
      fingerprint: targetFp.short,
    });
    assertEquals(
      fingerprintFallback,
      target,
      "fingerprint locator should resolve element when selector is absent",
    );
  });
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "fingerprint ignores volatile attributes",
      fn: testFingerprintIgnoresVolatileAttributes,
    },
    {
      name: "fingerprint determinism with noisy children",
      fn: testFingerprintDeterminismWithNoisyChildren,
    },
    {
      name: "fingerprint text cache consistency",
      fn: testFingerprintTextCacheConsistency,
    },
    {
      name: "find and resolve by fingerprint",
      fn: testFindAndResolveByFingerprint,
    },
  ];
  await runTests("fingerprintDom.test.ts", tests);
}
