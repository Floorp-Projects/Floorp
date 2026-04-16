// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { extractPlainText } from "../../src/lib/extractText.ts";
import { migrateLexicalContent } from "../../src/lib/migrateLexicalToTiptap.ts";
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
  const actualJson = JSON.stringify(actual);
  const expectedJson = JSON.stringify(expected);
  if (actualJson !== expectedJson) {
    throw new Error(
      `${message}: expected=${expectedJson} actual=${actualJson}`,
    );
  }
}

const tests: TestCase[] = [
  {
    name: "extractPlainText from TipTap and Lexical",
    fn: () => {
      const tiptapContent = JSON.stringify({
        type: "doc",
        content: [
          { type: "paragraph", content: [{ type: "text", text: "Hello" }] },
          { type: "paragraph", content: [{ type: "text", text: "World" }] },
        ],
      });
      assertEquals(
        extractPlainText(tiptapContent),
        "HelloWorld",
        "TipTap doc should be flattened to plain text",
      );

      const lexicalContent = JSON.stringify({
        root: {
          children: [
            { type: "paragraph", children: [{ type: "text", text: "Lex" }] },
            { type: "paragraph", children: [{ type: "text", text: "ical" }] },
          ],
        },
      });
      assertEquals(
        extractPlainText(lexicalContent),
        "Lexical",
        "Lexical root should be flattened to plain text",
      );
    },
  },
  {
    name: "extractPlainText fallback behavior",
    fn: () => {
      assertEquals(
        extractPlainText("not json"),
        "not json",
        "invalid JSON should be returned as-is",
      );
      assertEquals(
        extractPlainText(JSON.stringify({ hello: "world" })),
        "[object Object]",
        "unknown JSON payload should fall back to String(parsed)",
      );
    },
  },
  {
    name: "migrateLexicalContent for structured nodes",
    fn: () => {
      const lexical = {
        root: {
          children: [
            {
              type: "heading",
              tag: "h2",
              format: "center",
              children: [{ type: "text", text: "Title", format: 3 }],
            },
            {
              type: "paragraph",
              format: "right",
              children: [
                { type: "text", text: "Line A", format: 0 },
                { type: "linebreak" },
                { type: "text", text: "Line B", format: 4 },
              ],
            },
            {
              type: "list",
              listType: "number",
              children: [
                {
                  type: "listitem",
                  children: [{ type: "text", text: "List item", format: 0 }],
                },
              ],
            },
            {
              type: "quote",
              children: [{ type: "text", text: "Quoted", format: 0 }],
            },
            {
              type: "mystery",
              children: [{ type: "text", text: "Unknown fallback", format: 0 }],
            },
          ],
        },
      };

      const migrated = migrateLexicalContent(JSON.stringify(lexical));
      assert(
        migrated !== undefined,
        "valid lexical content should be converted",
      );
      assertEquals(migrated.type, "doc", "converted root should be TipTap doc");

      const content = migrated.content;
      assert(
        Array.isArray(content),
        "converted doc should contain content array",
      );
      assertEquals(
        content.length,
        5,
        "all top-level lexical nodes should be converted",
      );

      const heading = content[0];
      assertEquals(
        heading.type,
        "heading",
        "heading node should be converted to heading",
      );
      assertDeepEquals(
        heading.attrs,
        { level: 2, textAlign: "center" },
        "heading attrs should include level and alignment",
      );

      const headingText = heading.content?.[0];
      assertEquals(headingText?.type, "text", "heading should keep text child");
      assertDeepEquals(
        headingText?.marks,
        [{ type: "bold" }, { type: "italic" }],
        "format bitmask should convert to bold+italic marks",
      );

      const paragraph = content[1];
      assertEquals(
        paragraph.type,
        "paragraph",
        "paragraph should remain paragraph",
      );
      assertDeepEquals(
        paragraph.attrs,
        { textAlign: "right" },
        "paragraph alignment should be preserved",
      );
      assertEquals(
        paragraph.content?.[1]?.type,
        "hardBreak",
        "linebreak should convert to hardBreak",
      );
      assertDeepEquals(
        paragraph.content?.[2]?.marks,
        [{ type: "strike" }],
        "strike bit should become strike mark",
      );

      const list = content[2];
      assertEquals(
        list.type,
        "orderedList",
        "number list should map to orderedList",
      );
      assertEquals(
        list.content?.[0]?.type,
        "listItem",
        "list child should map to listItem",
      );
      assertEquals(
        list.content?.[0]?.content?.[0]?.type,
        "paragraph",
        "listItem should wrap children with paragraph",
      );

      const quote = content[3];
      assertEquals(
        quote.type,
        "blockquote",
        "quote should convert to blockquote",
      );

      const fallback = content[4];
      assertEquals(
        fallback.type,
        "paragraph",
        "unknown node type should fall back to paragraph",
      );
      assertEquals(
        fallback.content?.[0]?.type,
        "text",
        "unknown node children should still be converted",
      );
    },
  },
  {
    name: "migrateLexicalContent for plain text and edge cases",
    fn: () => {
      assertEquals(
        migrateLexicalContent(undefined),
        undefined,
        "undefined content should stay undefined",
      );
      assertEquals(
        migrateLexicalContent("   "),
        undefined,
        "blank content should stay undefined",
      );

      const tiptap = {
        type: "doc",
        content: [
          { type: "paragraph", content: [{ type: "text", text: "ready" }] },
        ],
      };
      const tiptapJson = JSON.stringify(tiptap);
      assertDeepEquals(
        migrateLexicalContent(tiptapJson),
        tiptap,
        "existing TipTap JSON should pass through unchanged",
      );

      const plain = migrateLexicalContent("Line 1\n\nLine 3");
      assert(plain !== undefined, "plain text should be converted");
      assertEquals(
        plain.content?.length,
        3,
        "newline-separated text should become paragraph-per-line",
      );
      assertEquals(
        plain.content?.[0]?.content?.[0]?.text,
        "Line 1",
        "first line text should be preserved",
      );
      assertEquals(
        plain.content?.[1]?.type,
        "paragraph",
        "blank line should become empty paragraph",
      );
      assertEquals(
        plain.content?.[2]?.content?.[0]?.text,
        "Line 3",
        "third line text should be preserved",
      );

      const unknownJson = migrateLexicalContent(JSON.stringify({ foo: "bar" }));
      assert(
        unknownJson !== undefined,
        "unknown JSON should still convert to plain text doc",
      );
      assertEquals(
        unknownJson.content?.[0]?.content?.[0]?.text,
        '{"foo":"bar"}',
        "unknown JSON should be treated as raw text payload",
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("contentMigration.test.ts", tests);
}
