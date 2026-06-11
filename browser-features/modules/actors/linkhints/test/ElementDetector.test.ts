// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { ElementDetector } from "../ElementDetector.ts";
import type { ClickableElementInfo } from "../types.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

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

/**
 * Create a container, append to body, run the test callback, then clean up.
 * The container is positioned at (0,0) with explicit dimensions so children
 * are guaranteed to be in the viewport.
 */
function withFixture(run: (container: HTMLElement) => void): void {
  const doc = requireDocument();
  const container = doc.createElement("div");
  container.style.setProperty("position", "fixed");
  container.style.setProperty("top", "0");
  container.style.setProperty("left", "0");
  container.style.setProperty("width", "500px");
  container.style.setProperty("height", "500px");
  container.style.setProperty("z-index", "99999");
  container.setAttribute("data-test-root", "element-detector");
  requireBody(doc).appendChild(container);
  try {
    run(container);
  } finally {
    container.remove();
  }
}

/**
 * Create a visible element inside the fixture container with default styles
 * that guarantee visibility and viewport presence.
 */
function createVisibleElement(doc: Document, tag: string, extraAttrs?: Record<string, string>): HTMLElement {
  const el = doc.createElement(tag) as HTMLElement;
  el.style.setProperty("position", "absolute");
  el.style.setProperty("top", "0");
  el.style.setProperty("left", "0");
  el.style.setProperty("width", "100px");
  el.style.setProperty("height", "40px");
  if (extraAttrs) {
    for (const [key, value] of Object.entries(extraAttrs)) {
      el.setAttribute(key, value);
    }
  }
  return el;
}

/**
 * Run the detector on the full window and return results filtered to elements
 * that are inside the test container.
 */
function detectInContainer(doc: Document, container: HTMLElement): ClickableElementInfo[] {
  const detector = new ElementDetector();
  const allResults = detector.detect(doc.defaultView!);
  const containerRect = container.getBoundingClientRect();
  return allResults.filter((info) => {
    return (
      info.rect.left >= containerRect.left &&
      info.rect.top >= containerRect.top &&
      info.rect.right <= containerRect.right + 2 &&
      info.rect.bottom <= containerRect.bottom + 2
    );
  });
}

// ---------------------------------------------------------------------------
// Test: detects anchor elements with href
// ---------------------------------------------------------------------------
function testDetectsAnchorWithHref(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const anchor = createVisibleElement(doc, "a");
    anchor.setAttribute("href", "https://example.com");
    anchor.textContent = "Click me";
    container.appendChild(anchor);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "a");
    assert(found, "should detect <a> element with href");
  });
}

// ---------------------------------------------------------------------------
// Test: detects anchor elements without href
// ---------------------------------------------------------------------------
function testDetectsAnchorWithoutHref(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const anchor = createVisibleElement(doc, "a");
    anchor.textContent = "Anchor text";
    container.appendChild(anchor);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "a");
    assert(found, "should detect <a> element even without href");
  });
}

// ---------------------------------------------------------------------------
// Test: detects button elements
// ---------------------------------------------------------------------------
function testDetectsButton(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const button = createVisibleElement(doc, "button");
    button.textContent = "Submit";
    container.appendChild(button);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "button");
    assert(found, "should detect <button> element");
  });
}

// ---------------------------------------------------------------------------
// Test: detects input elements (non-hidden)
// ---------------------------------------------------------------------------
function testDetectsInput(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const input = createVisibleElement(doc, "input");
    input.setAttribute("type", "text");
    container.appendChild(input);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "input");
    assert(found, "should detect <input type='text'> element");
  });
}

// ---------------------------------------------------------------------------
// Test: excludes hidden inputs
// ---------------------------------------------------------------------------
function testExcludesHiddenInput(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const input = createVisibleElement(doc, "input");
    input.setAttribute("type", "hidden");
    container.appendChild(input);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "input");
    assert(!found, "should NOT detect <input type='hidden'> element");
  });
}

// ---------------------------------------------------------------------------
// Test: excludes disabled elements
// ---------------------------------------------------------------------------
function testExcludesDisabledButton(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const button = createVisibleElement(doc, "button");
    button.setAttribute("disabled", "");
    button.textContent = "Disabled";
    container.appendChild(button);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "button");
    assert(!found, "should NOT detect disabled <button> element");
  });
}

// ---------------------------------------------------------------------------
// Test: detects elements with onclick
// ---------------------------------------------------------------------------
function testDetectsOnclick(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const div = createVisibleElement(doc, "div");
    div.setAttribute("onclick", "void(0)");
    div.textContent = "Clickable div";
    container.appendChild(div);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "div");
    assert(found, "should detect <div> with onclick attribute");
  });
}

// ---------------------------------------------------------------------------
// Test: detects elements with ARIA button role
// ---------------------------------------------------------------------------
function testDetectsAriaButtonRole(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const div = createVisibleElement(doc, "div");
    div.setAttribute("role", "button");
    div.textContent = "ARIA button";
    container.appendChild(div);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "div");
    assert(found, "should detect <div role='button'>");
  });
}

// ---------------------------------------------------------------------------
// Test: detects elements with ARIA link role
// ---------------------------------------------------------------------------
function testDetectsAriaLinkRole(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const span = createVisibleElement(doc, "span");
    span.setAttribute("role", "link");
    span.textContent = "ARIA link";
    container.appendChild(span);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "span");
    assert(found, "should detect <span role='link'>");
  });
}

// ---------------------------------------------------------------------------
// Test: excludes elements with display:none
// ---------------------------------------------------------------------------
function testExcludesDisplayNone(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const anchor = createVisibleElement(doc, "a");
    anchor.setAttribute("href", "https://example.com");
    anchor.style.setProperty("display", "none");
    container.appendChild(anchor);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "a");
    assert(!found, "should NOT detect <a> with display:none");
  });
}

// ---------------------------------------------------------------------------
// Test: excludes elements with visibility:hidden
// ---------------------------------------------------------------------------
function testExcludesVisibilityHidden(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const anchor = createVisibleElement(doc, "a");
    anchor.setAttribute("href", "https://example.com");
    anchor.style.setProperty("visibility", "hidden");
    container.appendChild(anchor);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "a");
    assert(!found, "should NOT detect <a> with visibility:hidden");
  });
}

// ---------------------------------------------------------------------------
// Test: excludes elements with opacity:0
// ---------------------------------------------------------------------------
function testExcludesOpacityZero(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const anchor = createVisibleElement(doc, "a");
    anchor.setAttribute("href", "https://example.com");
    anchor.style.setProperty("opacity", "0");
    container.appendChild(anchor);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "a");
    assert(!found, "should NOT detect <a> with opacity:0");
  });
}

// ---------------------------------------------------------------------------
// Test: excludes elements outside viewport
// ---------------------------------------------------------------------------
function testExcludesOutsideViewport(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const anchor = createVisibleElement(doc, "a");
    anchor.setAttribute("href", "https://example.com");
    anchor.style.setProperty("top", "-9999px");
    container.appendChild(anchor);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "a");
    assert(!found, "should NOT detect <a> positioned outside viewport");
  });
}

// ---------------------------------------------------------------------------
// Test: detects elements with tabindex >= 0
// ---------------------------------------------------------------------------
function testDetectsTabindex(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const div = createVisibleElement(doc, "div");
    div.setAttribute("tabindex", "0");
    div.textContent = "Focusable div";
    container.appendChild(div);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "div");
    assert(found, "should detect <div tabindex='0'>");
  });
}

// ---------------------------------------------------------------------------
// Test: detects elements with contentEditable
// ---------------------------------------------------------------------------
function testDetectsContentEditable(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const div = createVisibleElement(doc, "div");
    div.setAttribute("contenteditable", "true");
    div.textContent = "Editable content";
    container.appendChild(div);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "div");
    assert(found, "should detect <div contenteditable='true'>");
  });
}

// ---------------------------------------------------------------------------
// Test: detects select elements
// ---------------------------------------------------------------------------
function testDetectsSelect(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const select = createVisibleElement(doc, "select");
    const option = doc.createElement("option");
    option.textContent = "Option 1";
    select.appendChild(option);
    container.appendChild(select);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "select");
    assert(found, "should detect <select> element");
  });
}

// ---------------------------------------------------------------------------
// Test: detects textarea elements
// ---------------------------------------------------------------------------
function testDetectsTextarea(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const textarea = createVisibleElement(doc, "textarea");
    textarea.textContent = "Text area";
    container.appendChild(textarea);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "textarea");
    assert(found, "should detect <textarea> element");
  });
}

// ---------------------------------------------------------------------------
// Test: detects details elements
// ---------------------------------------------------------------------------
function testDetectsDetails(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const details = createVisibleElement(doc, "details");
    const summary = doc.createElement("summary");
    summary.textContent = "Details summary";
    details.appendChild(summary);
    container.appendChild(details);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "details");
    assert(found, "should detect <details> element");
  });
}

// ---------------------------------------------------------------------------
// Test: detects elements with jsaction click
// ---------------------------------------------------------------------------
function testDetectsJsactionClick(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const div = createVisibleElement(doc, "div");
    div.setAttribute("jsaction", "click:example.action");
    div.textContent = "JS action div";
    container.appendChild(div);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "div");
    assert(found, "should detect <div> with jsaction click");
  });
}

// ---------------------------------------------------------------------------
// Test: excludes elements with aria-disabled="true"
// ---------------------------------------------------------------------------
function testExcludesAriaDisabled(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const button = createVisibleElement(doc, "button");
    button.setAttribute("aria-disabled", "true");
    button.textContent = "ARIA disabled";
    container.appendChild(button);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "button");
    assert(!found, "should NOT detect <button aria-disabled='true'>");
  });
}

// ---------------------------------------------------------------------------
// Test: sorts results by position (top-to-bottom, left-to-right)
// ---------------------------------------------------------------------------
function testSortsByPosition(): void {
  withFixture((container) => {
    const doc = requireDocument();

    // Create elements at different positions
    const topRight = createVisibleElement(doc, "a");
    topRight.setAttribute("href", "#top-right");
    topRight.style.setProperty("top", "10px");
    topRight.style.setProperty("left", "200px");
    topRight.textContent = "Top Right";
    container.appendChild(topRight);

    const bottomLeft = createVisibleElement(doc, "a");
    bottomLeft.setAttribute("href", "#bottom-left");
    bottomLeft.style.setProperty("top", "100px");
    bottomLeft.style.setProperty("left", "10px");
    bottomLeft.textContent = "Bottom Left";
    container.appendChild(bottomLeft);

    const topLeft = createVisibleElement(doc, "a");
    topLeft.setAttribute("href", "#top-left");
    topLeft.style.setProperty("top", "10px");
    topLeft.style.setProperty("left", "10px");
    topLeft.textContent = "Top Left";
    container.appendChild(topLeft);

    const results = detectInContainer(doc, container);
    assertEquals(results.length, 3, "should detect all 3 anchor elements");

    // Top elements (index 0 and 1) should come before bottom (index 2)
    assert(results[2].rect.top > results[0].rect.top, "third element should be below first element");
    assert(results[2].rect.top > results[1].rect.top, "third element should be below second element");

    // Top-left should come before top-right
    if (Math.abs(results[0].rect.top - results[1].rect.top) <= 10) {
      assert(
        results[0].rect.left <= results[1].rect.left,
        "among top elements, leftmost should come first",
      );
    }
  });
}

// ---------------------------------------------------------------------------
// Test: extracts href from anchor elements
// ---------------------------------------------------------------------------
function testExtractsHrefFromAnchor(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const anchor = createVisibleElement(doc, "a");
    anchor.setAttribute("href", "https://example.com/test");
    anchor.textContent = "Test Link";
    container.appendChild(anchor);

    const results = detectInContainer(doc, container);
    assert(results.length >= 1, "should detect the anchor");
    const link = results.find((r) => r.tagName === "a");
    assert(link !== undefined, "should find an anchor in results");
    assert(
      link!.href !== null && link!.href.includes("example.com/test"),
      "should extract href from <a> element",
    );
  });
}

// ---------------------------------------------------------------------------
// Test: extracts href from element inside anchor
// ---------------------------------------------------------------------------
function testExtractsHrefFromNestedElement(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const anchor = createVisibleElement(doc, "a");
    anchor.setAttribute("href", "https://example.com/nested");

    const span = doc.createElement("span");
    span.textContent = "Nested text";
    anchor.appendChild(span);
    container.appendChild(anchor);

    const results = detectInContainer(doc, container);
    // Both the anchor and the span may be detected; span should inherit href
    const nestedLink = results.find((r) => r.tagName === "span");
    if (nestedLink) {
      assert(
        nestedLink.href !== null && nestedLink.href.includes("example.com/nested"),
        "nested <span> inside <a> should inherit href",
      );
    } else {
      // If span is not independently clickable, the anchor should still be found
      const anchorResult = results.find((r) => r.tagName === "a");
      assert(
        anchorResult !== undefined && anchorResult!.href !== null,
        "anchor should at least be detected with href",
      );
    }
  });
}

// ---------------------------------------------------------------------------
// Test: text content is extracted
// ---------------------------------------------------------------------------
function testExtractsTextContent(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const button = createVisibleElement(doc, "button");
    button.textContent = "Submit Button";
    container.appendChild(button);

    const results = detectInContainer(doc, container);
    const btn = results.find((r) => r.tagName === "button");
    assert(btn !== undefined, "should find button in results");
    assertEquals(btn!.text, "Submit Button", "should extract text content from button");
  });
}

// ---------------------------------------------------------------------------
// Test: excludes elements with zero dimensions
// ---------------------------------------------------------------------------
function testExcludesZeroDimensions(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const anchor = createVisibleElement(doc, "a");
    anchor.setAttribute("href", "https://example.com");
    anchor.style.setProperty("width", "0");
    anchor.style.setProperty("height", "0");
    container.appendChild(anchor);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "a");
    assert(!found, "should NOT detect element with 0x0 dimensions");
  });
}

// ---------------------------------------------------------------------------
// Test: excludes non-HTMLElement nodes
// ---------------------------------------------------------------------------
function testExcludesNonHTMLElement(): void {
  withFixture((container) => {
    const doc = requireDocument();
    // SVG elements are not HTMLElement in some contexts
    const svg = doc.createElementNS("http://www.w3.org/2000/svg", "svg");
    svg.style.setProperty("position", "absolute");
    svg.style.setProperty("width", "100px");
    svg.style.setProperty("height", "100px");
    const rect = doc.createElementNS("http://www.w3.org/2000/svg", "rect");
    rect.setAttribute("width", "100");
    rect.setAttribute("height", "100");
    svg.appendChild(rect);
    container.appendChild(svg);

    // This should not crash; SVG elements are not HTMLElement
    const detector = new ElementDetector();
    const results = detector.detect(doc.defaultView!);
    assert(Array.isArray(results), "should return an array without crashing");
  });
}

// ---------------------------------------------------------------------------
// Test: detects label with valid for attribute
// ---------------------------------------------------------------------------
function testDetectsLabelWithFor(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const input = createVisibleElement(doc, "input");
    input.setAttribute("type", "checkbox");
    input.setAttribute("id", "test-checkbox");
    container.appendChild(input);

    const label = createVisibleElement(doc, "label");
    label.setAttribute("for", "test-checkbox");
    label.textContent = "Check me";
    label.style.setProperty("top", "50px");
    container.appendChild(label);

    const results = detectInContainer(doc, container);
    const found = results.some((r) => r.tagName === "label");
    assert(found, "should detect <label> with valid 'for' attribute pointing to enabled input");
  });
}

// ---------------------------------------------------------------------------
// Test: possibleFalsePositive for button class heuristic
// ---------------------------------------------------------------------------
function testButtonClassHeuristic(): void {
  withFixture((container) => {
    const doc = requireDocument();
    const div = createVisibleElement(doc, "div");
    div.setAttribute("class", "btn-primary");
    div.textContent = "Button-like div";
    container.appendChild(div);

    const results = detectInContainer(doc, container);
    const found = results.find((r) => r.tagName === "div");
    assert(found !== undefined, "should detect <div> with 'btn' class name");
    assert(
      found!.possibleFalsePositive,
      "should mark class-heuristic elements as possibleFalsePositive",
    );
  });
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "detects anchor elements with href", fn: testDetectsAnchorWithHref },
    { name: "detects anchor elements without href", fn: testDetectsAnchorWithoutHref },
    { name: "detects button elements", fn: testDetectsButton },
    { name: "detects input elements (non-hidden)", fn: testDetectsInput },
    { name: "excludes hidden inputs", fn: testExcludesHiddenInput },
    { name: "excludes disabled elements", fn: testExcludesDisabledButton },
    { name: "detects elements with onclick", fn: testDetectsOnclick },
    { name: "detects elements with ARIA button role", fn: testDetectsAriaButtonRole },
    { name: "detects elements with ARIA link role", fn: testDetectsAriaLinkRole },
    { name: "excludes elements with display:none", fn: testExcludesDisplayNone },
    { name: "excludes elements with visibility:hidden", fn: testExcludesVisibilityHidden },
    { name: "excludes elements with opacity:0", fn: testExcludesOpacityZero },
    { name: "excludes elements outside viewport", fn: testExcludesOutsideViewport },
    { name: "detects elements with tabindex", fn: testDetectsTabindex },
    { name: "detects elements with contentEditable", fn: testDetectsContentEditable },
    { name: "detects select elements", fn: testDetectsSelect },
    { name: "detects textarea elements", fn: testDetectsTextarea },
    { name: "detects details elements", fn: testDetectsDetails },
    { name: "detects elements with jsaction click", fn: testDetectsJsactionClick },
    { name: "excludes elements with aria-disabled", fn: testExcludesAriaDisabled },
    { name: "sorts results by position (top-to-bottom, left-to-right)", fn: testSortsByPosition },
    { name: "extracts href from anchor elements", fn: testExtractsHrefFromAnchor },
    { name: "extracts href from element inside anchor", fn: testExtractsHrefFromNestedElement },
    { name: "extracts text content", fn: testExtractsTextContent },
    { name: "excludes elements with zero dimensions", fn: testExcludesZeroDimensions },
    { name: "handles non-HTMLElement nodes without crashing", fn: testExcludesNonHTMLElement },
    { name: "detects label with valid for attribute", fn: testDetectsLabelWithFor },
    { name: "marks button class heuristic as possibleFalsePositive", fn: testButtonClassHeuristic },
  ];
  await runTests("ElementDetector.test.ts", tests);
}
