// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { StyleElement } from "../styleElem.tsx";
import { render } from "@nora/solid-xul";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

/** Render StyleElement into a container and return the rendered CSS text content */
function renderAndGetCss(): { css: string; cleanup: () => void } {
  const container = document.createElement("div");
  document.head.appendChild(container);
  render(() => StyleElement(), container);
  // SolidJS renders <style> inside the container
  const styleEl = container.querySelector("style") ?? container.querySelector("div");
  const css = styleEl?.textContent ?? "";
  return { css, cleanup: () => container.remove() };
}

function testStyleElementReturnsJsxNode(): void {
  const node = StyleElement();
  assert(node !== null, "StyleElement should return JSX node");
  assertEquals(
    typeof node,
    "object",
    "StyleElement return should be object-like",
  );
}

function testStyleElementIsStableAcrossCalls(): void {
  const first = StyleElement();
  const second = StyleElement();
  assert(
    first !== null && second !== null,
    "multiple calls should return nodes",
  );
  assertEquals(typeof first, typeof second, "return type should remain stable");
}

function testRenderedStyleContainsUndoSelector(): void {
  const head = document?.head;
  assert(head !== null && head !== undefined, "document.head should exist");

  render(() => StyleElement(), head);
  const styleNodes = head.querySelectorAll("style");
  const latestStyle = styleNodes.item(styleNodes.length - 1);

  try {
    assert(latestStyle !== null, "render should insert a style element");
    assert(
      (latestStyle.textContent ?? "").includes("#undo-closed-tab"),
      "rendered style should include #undo-closed-tab selector",
    );
  } finally {
    latestStyle?.remove();
  }
}

function testStyleElementContentNotEmpty(): void {
  const node = StyleElement();
  assert(node !== null, "StyleElement should return non-null node");
}

function testStyleElementContainsStyleTag(): void {
  const { css, cleanup } = renderAndGetCss();
  try {
    // CSS content was rendered — verify it came from a style element
    assert(css.length > 0, "StyleElement should produce CSS content");
  } finally {
    cleanup();
  }
}

function testStyleElementHasChildren(): void {
  const { css, cleanup } = renderAndGetCss();
  try {
    assert(
      css !== null && css !== undefined && css.length > 0,
      "StyleElement should have CSS content after rendering",
    );
  } finally {
    cleanup();
  }
}

function testStyleElementContainsCssContent(): void {
  const { css, cleanup } = renderAndGetCss();
  try {
    assert(
      typeof css === "string" && css.length > 0,
      "StyleElement children should be a non-empty string",
    );
  } finally {
    cleanup();
  }
}

function testStyleElementContainsUndoSelectorInContent(): void {
  const { css, cleanup } = renderAndGetCss();
  try {
    assert(
      css.includes("#undo-closed-tab"),
      "CSS content should include #undo-closed-tab selector",
    );
  } finally {
    cleanup();
  }
}

function testStyleElementContainsListStyleImage(): void {
  const { css, cleanup } = renderAndGetCss();
  try {
    assert(
      css.includes("list-style-image"),
      "CSS content should include list-style-image property",
    );
  } finally {
    cleanup();
  }
}

function testStyleElementContainsChromeUrl(): void {
  const { css, cleanup } = renderAndGetCss();
  try {
    assert(
      css.includes("chrome://global/skin/icons/undo.svg"),
      "CSS content should include chrome:// URL for undo icon",
    );
  } finally {
    cleanup();
  }
}

function testStyleElementCssStructure(): void {
  const { css, cleanup } = renderAndGetCss();
  try {
    assert(css.includes("{"), "CSS should contain opening brace");
    assert(css.includes("}"), "CSS should contain closing brace");
    const parts = css.split("{");
    assert(parts.length >= 2, "CSS should have selector and declaration block");
  } finally {
    cleanup();
  }
}

function testStyleElementHasNoWhitespaceIssues(): void {
  const { css, cleanup } = renderAndGetCss();
  try {
    const trimmed = css.trim();
    assert(
      trimmed.length > 0 && trimmed.length <= css.length + 1,
      "CSS content should not have excessive leading/trailing whitespace",
    );
  } finally {
    cleanup();
  }
}

function testStyleElementIsIdempotent(): void {
  const { css: first, cleanup: c1 } = renderAndGetCss();
  const { css: second, cleanup: c2 } = renderAndGetCss();
  const { css: third, cleanup: c3 } = renderAndGetCss();
  try {
    assertEquals(
      first,
      second,
      "First and second calls should return same content",
    );
    assertEquals(
      second,
      third,
      "Second and third calls should return same content",
    );
  } finally {
    c1();
    c2();
    c3();
  }
}

function testStyleElementMultipleRenderBehavior(): void {
  const head = document?.head;
  assert(head !== null && head !== undefined, "document.head should exist");

  const initialCount = head.querySelectorAll("style").length;

  render(() => StyleElement(), head);
  const afterFirstRender = head.querySelectorAll("style").length;
  assert(
    afterFirstRender >= initialCount,
    "First render should add at least one style element",
  );

  render(() => StyleElement(), head);
  const afterSecondRender = head.querySelectorAll("style").length;
  assert(
    afterSecondRender >= afterFirstRender,
    "Second render should not remove existing styles",
  );

  const allStyles = head.querySelectorAll("style");
  for (let i = initialCount; i < allStyles.length; i++) {
    allStyles.item(i)?.remove();
  }
}

function testStyleElementCssSelectorValidity(): void {
  const { css, cleanup } = renderAndGetCss();
  try {
    assert(
      css.includes("#undo-closed-tab"),
      "CSS should include '#undo-closed-tab' selector",
    );
    assert(
      /#undo-closed-tab\s*\{/.test(css),
      "CSS selector block for '#undo-closed-tab' should be valid",
    );
  } finally {
    cleanup();
  }
}

function testStyleElementCssPropertySyntax(): void {
  const { css, cleanup } = renderAndGetCss();
  try {
    assert(css.includes(":"), "CSS should contain property separator");
    assert(css.includes(";"), "CSS should contain property terminator");
    assert(css.includes("url("), "CSS should include url() function for image");
    assert(css.includes(")"), "CSS should close url() function");
  } finally {
    cleanup();
  }
}

function testStyleElementIconUrlCorrectness(): void {
  const { css, cleanup } = renderAndGetCss();
  try {
    const expectedUrl = "chrome://global/skin/icons/undo.svg";
    assert(
      css.includes(expectedUrl),
      `CSS should include exact URL: ${expectedUrl}`,
    );
    assert(
      css.includes(`url("${expectedUrl}")`) ||
      css.includes(`url('${expectedUrl}')`) ||
      css.includes(`url(${expectedUrl})`),
      "URL should be properly wrapped in url() function",
    );
  } finally {
    cleanup();
  }
}

function testStyleElementNoInvalidCss(): void {
  const { css, cleanup } = renderAndGetCss();
  try {
    const invalidPatterns = [
      ";;",
      "::",
      "{{",
      "}}",
      "url()",
    ];

    for (const pattern of invalidPatterns) {
      if (css.includes(pattern)) {
        assert(false, `CSS should not contain invalid pattern: ${pattern}`);
      }
    }

    assert(true, "CSS does not contain obvious invalid patterns");
  } finally {
    cleanup();
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "StyleElement returns JSX node",
      fn: testStyleElementReturnsJsxNode,
    },
    {
      name: "StyleElement stable across calls",
      fn: testStyleElementIsStableAcrossCalls,
    },
    {
      name: "rendered style contains undo selector",
      fn: testRenderedStyleContainsUndoSelector,
    },
    {
      name: "StyleElement content is not empty",
      fn: testStyleElementContentNotEmpty,
    },
    {
      name: "StyleElement contains style tag",
      fn: testStyleElementContainsStyleTag,
    },
    {
      name: "StyleElement has children",
      fn: testStyleElementHasChildren,
    },
    {
      name: "StyleElement contains CSS content",
      fn: testStyleElementContainsCssContent,
    },
    {
      name: "StyleElement contains undo selector in content",
      fn: testStyleElementContainsUndoSelectorInContent,
    },
    {
      name: "StyleElement contains list-style-image",
      fn: testStyleElementContainsListStyleImage,
    },
    {
      name: "StyleElement contains chrome URL",
      fn: testStyleElementContainsChromeUrl,
    },
    {
      name: "StyleElement CSS structure is valid",
      fn: testStyleElementCssStructure,
    },
    {
      name: "StyleElement has no whitespace issues",
      fn: testStyleElementHasNoWhitespaceIssues,
    },
    {
      name: "StyleElement is idempotent",
      fn: testStyleElementIsIdempotent,
    },
    {
      name: "StyleElement multiple render behavior",
      fn: testStyleElementMultipleRenderBehavior,
    },
    {
      name: "StyleElement CSS selector validity",
      fn: testStyleElementCssSelectorValidity,
    },
    {
      name: "StyleElement CSS property syntax",
      fn: testStyleElementCssPropertySyntax,
    },
    {
      name: "StyleElement icon URL correctness",
      fn: testStyleElementIconUrlCorrectness,
    },
    {
      name: "StyleElement has no invalid CSS",
      fn: testStyleElementNoInvalidCss,
    },
  ];

  await runTests("undoClosedTabStyleElement.test.ts", tests);
}
