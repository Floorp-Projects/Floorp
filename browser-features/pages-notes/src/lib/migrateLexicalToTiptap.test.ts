// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { migrateLexicalContent } from "./migrateLexicalToTiptap.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

function assertDeepEquals(
  actual: unknown,
  expected: unknown,
  message: string,
): void {
  if (JSON.stringify(actual) !== JSON.stringify(expected)) {
    throw new Error(
      `${message}\nexpected: ${JSON.stringify(expected)}\nactual:   ${JSON.stringify(actual)}`,
    );
  }
}

// ---------------------------------------------------------------------------
// Tests — empty/undefined input
// ---------------------------------------------------------------------------

function testUndefinedInput(): void {
  assertEquals(
    migrateLexicalContent(undefined),
    undefined,
    "undefined should return undefined",
  );
}

function testEmptyString(): void {
  assertEquals(
    migrateLexicalContent(""),
    undefined,
    "empty string should return undefined",
  );
}

function testWhitespaceOnly(): void {
  assertEquals(
    migrateLexicalContent("   "),
    undefined,
    "whitespace-only should return undefined",
  );
}

// ---------------------------------------------------------------------------
// Tests — TipTap passthrough
// ---------------------------------------------------------------------------

function testTiptapPassthrough(): void {
  const tiptap = {
    type: "doc",
    content: [{ type: "paragraph", content: [{ type: "text", text: "hi" }] }],
  };
  const result = migrateLexicalContent(JSON.stringify(tiptap));
  assert(result !== undefined, "should not be undefined");
  assertEquals(result.type, "doc", "should be doc type");
  assertDeepEquals(result, tiptap, "should pass through TipTap unchanged");
}

// ---------------------------------------------------------------------------
// Tests — Lexical conversion
// ---------------------------------------------------------------------------

function testLexicalParagraph(): void {
  const lexical = {
    root: {
      children: [
        {
          type: "paragraph",
          children: [{ type: "text", text: "Hello", format: 0 }],
        },
      ],
    },
  };
  const result = migrateLexicalContent(JSON.stringify(lexical));
  assert(result !== undefined, "should produce result");
  assertEquals(result.type, "doc", "root should be doc");
  assert(
    Array.isArray(result.content) && result.content.length === 1,
    "should have 1 paragraph",
  );
  assertEquals(result.content![0].type, "paragraph", "should be paragraph");
}

function testLexicalBoldText(): void {
  const lexical = {
    root: {
      children: [
        {
          type: "paragraph",
          children: [{ type: "text", text: "Bold", format: 1 }],
        },
      ],
    },
  };
  const result = migrateLexicalContent(JSON.stringify(lexical));
  const textNode = result!.content![0].content![0];
  assert(textNode.marks !== undefined, "should have marks");
  assert(
    textNode.marks!.some((m: { type: string }) => m.type === "bold"),
    "should have bold mark",
  );
}

function testLexicalItalicText(): void {
  const lexical = {
    root: {
      children: [
        {
          type: "paragraph",
          children: [{ type: "text", text: "Italic", format: 2 }],
        },
      ],
    },
  };
  const result = migrateLexicalContent(JSON.stringify(lexical));
  const textNode = result!.content![0].content![0];
  assert(
    textNode.marks!.some((m: { type: string }) => m.type === "italic"),
    "should have italic mark",
  );
}

function testLexicalBoldItalic(): void {
  const lexical = {
    root: {
      children: [
        {
          type: "paragraph",
          children: [{ type: "text", text: "Both", format: 3 }],
        },
      ],
    },
  };
  const result = migrateLexicalContent(JSON.stringify(lexical));
  const marks = result!.content![0].content![0].marks!;
  assertEquals(marks.length, 2, "should have 2 marks");
}

function testLexicalHeading(): void {
  const lexical = {
    root: {
      children: [
        {
          type: "heading",
          tag: "h2",
          children: [{ type: "text", text: "Title", format: 0 }],
        },
      ],
    },
  };
  const result = migrateLexicalContent(JSON.stringify(lexical));
  const heading = result!.content![0];
  assertEquals(heading.type, "heading", "should be heading");
  assertEquals(heading.attrs!.level, 2, "should be level 2");
}

function testLexicalBulletList(): void {
  const lexical = {
    root: {
      children: [
        {
          type: "list",
          listType: "bullet",
          children: [
            {
              type: "listitem",
              children: [{ type: "text", text: "Item 1", format: 0 }],
            },
          ],
        },
      ],
    },
  };
  const result = migrateLexicalContent(JSON.stringify(lexical));
  assertEquals(result!.content![0].type, "bulletList", "should be bulletList");
}

function testLexicalOrderedList(): void {
  const lexical = {
    root: {
      children: [
        {
          type: "list",
          listType: "number",
          children: [
            {
              type: "listitem",
              children: [{ type: "text", text: "Step 1", format: 0 }],
            },
          ],
        },
      ],
    },
  };
  const result = migrateLexicalContent(JSON.stringify(lexical));
  assertEquals(
    result!.content![0].type,
    "orderedList",
    "should be orderedList",
  );
}

function testLexicalBlockquote(): void {
  const lexical = {
    root: {
      children: [
        {
          type: "quote",
          children: [{ type: "text", text: "Quote", format: 0 }],
        },
      ],
    },
  };
  const result = migrateLexicalContent(JSON.stringify(lexical));
  assertEquals(result!.content![0].type, "blockquote", "should be blockquote");
}

function testLexicalLinebreak(): void {
  const lexical = {
    root: {
      children: [
        {
          type: "paragraph",
          children: [
            { type: "text", text: "Line1", format: 0 },
            { type: "linebreak" },
            { type: "text", text: "Line2", format: 0 },
          ],
        },
      ],
    },
  };
  const result = migrateLexicalContent(JSON.stringify(lexical));
  const content = result!.content![0].content!;
  assertEquals(content.length, 3, "should have 3 nodes");
  assertEquals(content[1].type, "hardBreak", "middle should be hardBreak");
}

function testLexicalEmptyRoot(): void {
  const lexical = { root: { children: [] } };
  const result = migrateLexicalContent(JSON.stringify(lexical));
  assert(result !== undefined, "should produce result");
  assertEquals(
    result.content![0].type,
    "paragraph",
    "should have empty paragraph fallback",
  );
}

// ---------------------------------------------------------------------------
// Tests — plain text
// ---------------------------------------------------------------------------

function testPlainText(): void {
  const result = migrateLexicalContent("Hello world");
  assert(result !== undefined, "should produce result");
  assertEquals(result.type, "doc", "should be doc");
  assertEquals(result.content![0].type, "paragraph", "should be paragraph");
  assertEquals(
    result.content![0].content![0].text,
    "Hello world",
    "text should match",
  );
}

function testPlainTextMultiline(): void {
  const result = migrateLexicalContent("Line1\nLine2\nLine3");
  assert(result !== undefined, "should produce result");
  assertEquals(
    result.content!.length,
    3,
    "should have 3 paragraphs for 3 lines",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "undefined input", fn: testUndefinedInput },
    { name: "empty string", fn: testEmptyString },
    { name: "whitespace only", fn: testWhitespaceOnly },
    { name: "TipTap passthrough", fn: testTiptapPassthrough },
    { name: "Lexical paragraph", fn: testLexicalParagraph },
    { name: "Lexical bold", fn: testLexicalBoldText },
    { name: "Lexical italic", fn: testLexicalItalicText },
    { name: "Lexical bold+italic", fn: testLexicalBoldItalic },
    { name: "Lexical heading", fn: testLexicalHeading },
    { name: "Lexical bullet list", fn: testLexicalBulletList },
    { name: "Lexical ordered list", fn: testLexicalOrderedList },
    { name: "Lexical blockquote", fn: testLexicalBlockquote },
    { name: "Lexical linebreak", fn: testLexicalLinebreak },
    { name: "Lexical empty root", fn: testLexicalEmptyRoot },
    { name: "plain text", fn: testPlainText },
    { name: "plain text multiline", fn: testPlainTextMultiline },
  ];

  await runTests("migrateLexicalToTiptap.test.ts", tests);
}
