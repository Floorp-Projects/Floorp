// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { extractPlainText } from "../../src/lib/extractText.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests — TipTap format
// ---------------------------------------------------------------------------

function testTiptapSimple(): void {
  const doc = {
    type: "doc",
    content: [
      {
        type: "paragraph",
        content: [{ type: "text", text: "Hello world" }],
      },
    ],
  };
  assertEquals(
    extractPlainText(JSON.stringify(doc)),
    "Hello world",
    "should extract text from TipTap doc",
  );
}

function testTiptapMultipleParagraphs(): void {
  const doc = {
    type: "doc",
    content: [
      { type: "paragraph", content: [{ type: "text", text: "Line1" }] },
      { type: "paragraph", content: [{ type: "text", text: "Line2" }] },
    ],
  };
  assertEquals(
    extractPlainText(JSON.stringify(doc)),
    "Line1Line2",
    "should concatenate paragraphs",
  );
}

function testTiptapNestedContent(): void {
  const doc = {
    type: "doc",
    content: [
      {
        type: "bulletList",
        content: [
          {
            type: "listItem",
            content: [
              {
                type: "paragraph",
                content: [{ type: "text", text: "Item" }],
              },
            ],
          },
        ],
      },
    ],
  };
  assertEquals(
    extractPlainText(JSON.stringify(doc)),
    "Item",
    "should extract nested text",
  );
}

function testTiptapEmptyDoc(): void {
  const doc = { type: "doc", content: [] };
  assertEquals(
    extractPlainText(JSON.stringify(doc)),
    "",
    "empty doc should return empty string",
  );
}

// ---------------------------------------------------------------------------
// Tests — Lexical format
// ---------------------------------------------------------------------------

function testLexicalFormat(): void {
  const lexical = {
    root: {
      children: [
        {
          type: "paragraph",
          children: [{ type: "text", text: "Lexical text" }],
        },
      ],
    },
  };
  assertEquals(
    extractPlainText(JSON.stringify(lexical)),
    "Lexical text",
    "should extract text from Lexical root",
  );
}

// ---------------------------------------------------------------------------
// Tests — plain text fallback
// ---------------------------------------------------------------------------

function testPlainTextFallback(): void {
  assertEquals(
    extractPlainText("Just plain text"),
    "Just plain text",
    "non-JSON should return as-is",
  );
}

function testEmptyString(): void {
  assertEquals(extractPlainText(""), "", "empty string should return empty");
}

function testJsonPrimitive(): void {
  assertEquals(
    extractPlainText('"a string"'),
    "a string",
    "JSON string should be stringified",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "TipTap simple", fn: testTiptapSimple },
    { name: "TipTap multiple paragraphs", fn: testTiptapMultipleParagraphs },
    { name: "TipTap nested content", fn: testTiptapNestedContent },
    { name: "TipTap empty doc", fn: testTiptapEmptyDoc },
    { name: "Lexical format", fn: testLexicalFormat },
    { name: "plain text fallback", fn: testPlainTextFallback },
    { name: "empty string", fn: testEmptyString },
    { name: "JSON primitive", fn: testJsonPrimitive },
  ];

  await runTests("extractText.test.ts", tests);
}
