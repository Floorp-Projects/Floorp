// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { noteToMarkdown, tiptapToMarkdown } from "../../src/lib/exportMarkdown.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

// deno-lint-ignore no-explicit-any
function doc(...content: unknown[]): any {
  return { type: "doc", content };
}
function para(text: string) {
  return { type: "paragraph", content: [{ type: "text", text }] };
}
function codeBlock(text: string, language?: string) {
  return {
    type: "codeBlock",
    ...(language ? { attrs: { language } } : {}),
    content: [{ type: "text", text }],
  };
}

// --------------------------------------------------------------- structure

function testEmptyDoc(): void {
  assertEquals(tiptapToMarkdown(doc()), "", "empty doc yields empty string");
  assertEquals(tiptapToMarkdown(undefined), "", "undefined doc yields empty string");
}

function testParagraphs(): void {
  assertEquals(
    tiptapToMarkdown(doc(para("First"), para("Second"))),
    "First\n\nSecond\n",
    "paragraphs separated by a blank line",
  );
}

function testHeadings(): void {
  assertEquals(
    tiptapToMarkdown(doc({
      type: "heading",
      attrs: { level: 2 },
      content: [{ type: "text", text: "Title" }],
    })),
    "## Title\n",
    "heading level 2",
  );
}

function testBulletList(): void {
  assertEquals(
    tiptapToMarkdown(doc({
      type: "bulletList",
      content: [
        { type: "listItem", content: [para("one")] },
        { type: "listItem", content: [para("two")] },
      ],
    })),
    "- one\n- two\n",
    "flat bullet list",
  );
}

function testNestedList(): void {
  assertEquals(
    tiptapToMarkdown(doc({
      type: "bulletList",
      content: [{
        type: "listItem",
        content: [
          para("outer"),
          {
            type: "bulletList",
            content: [{ type: "listItem", content: [para("inner")] }],
          },
        ],
      }],
    })),
    "- outer\n\n  - inner\n",
    "nested list is indented by the marker width",
  );
}

function testEmptyListItemHasNoTrailingSpace(): void {
  assertEquals(
    tiptapToMarkdown(doc({
      type: "bulletList",
      content: [
        { type: "listItem", content: [{ type: "paragraph" }] },
        { type: "listItem", content: [para("two")] },
      ],
    })),
    "-\n- two\n",
    "an empty list item must not leave a dangling space",
  );
}

function testOrderedListRespectsStart(): void {
  assertEquals(
    tiptapToMarkdown(doc({
      type: "orderedList",
      attrs: { start: 3 },
      content: [
        { type: "listItem", content: [para("a")] },
        { type: "listItem", content: [para("b")] },
      ],
    })),
    "3. a\n4. b\n",
    "ordered list honours the start attribute",
  );
}

function testBlockquote(): void {
  assertEquals(
    tiptapToMarkdown(doc({ type: "blockquote", content: [para("quoted")] })),
    "> quoted\n",
    "blockquote prefixes each line",
  );
}

function testHorizontalRuleAndHardBreak(): void {
  assertEquals(
    tiptapToMarkdown(doc({ type: "horizontalRule" })),
    "---\n",
    "horizontal rule",
  );
  assertEquals(
    tiptapToMarkdown(doc({
      type: "paragraph",
      content: [
        { type: "text", text: "a" },
        { type: "hardBreak" },
        { type: "text", text: "b" },
      ],
    })),
    "a  \nb\n",
    "hard break is two spaces then newline",
  );
}

function testDanglingHardBreakIsTrimmed(): void {
  assertEquals(
    tiptapToMarkdown(doc(
      { type: "paragraph", content: [{ type: "text", text: "end" }, { type: "hardBreak" }] },
      para("next"),
    )),
    "end\n\nnext\n",
    "a trailing hard break must not add a third newline between blocks",
  );
}

function testImage(): void {
  assertEquals(
    tiptapToMarkdown(doc({
      type: "image",
      attrs: { src: "data:image/jpeg;base64,AAAA", alt: "shot" },
    })),
    "![shot](data:image/jpeg;base64,AAAA)\n",
    "image becomes an inline markdown image",
  );
}

function testExternalImageSrcIsPreserved(): void {
  assertEquals(
    tiptapToMarkdown(doc({
      type: "image",
      attrs: { src: "https://example.com/a.png", alt: "" },
    })),
    "![](https://example.com/a.png)\n",
    "an external src survives verbatim; images are not always data URIs",
  );
}

function testTextAlignIsDropped(): void {
  assertEquals(
    tiptapToMarkdown(doc({
      type: "paragraph",
      attrs: { textAlign: "center" },
      content: [{ type: "text", text: "centred" }],
    })),
    "centred\n",
    "textAlign attribute is ignored",
  );
}

// ------------------------------------------------------------------- marks

function testMarks(): void {
  assertEquals(
    tiptapToMarkdown(doc({
      type: "paragraph",
      content: [
        { type: "text", text: "b", marks: [{ type: "bold" }] },
        { type: "text", text: "i", marks: [{ type: "italic" }] },
        { type: "text", text: "s", marks: [{ type: "strike" }] },
        { type: "text", text: "u", marks: [{ type: "underline" }] },
      ],
    })),
    "**b***i*~~s~~<u>u</u>\n",
    "bold, italic, strike, underline",
  );
}

function testInlineCodeIsNotEscaped(): void {
  assertEquals(
    tiptapToMarkdown(doc({
      type: "paragraph",
      content: [{ type: "text", text: "a*b", marks: [{ type: "code" }] }],
    })),
    "`a*b`\n",
    "code mark content is not escaped",
  );
}

// ---------------------------------------------------------------- escaping

function testTextEscaping(): void {
  assertEquals(
    tiptapToMarkdown(doc(para("5 * 3 _x_ [y]"))),
    "5 \\* 3 \\_x\\_ \\[y\\]\n",
    "markdown specials in plain text are escaped",
  );
}

function testTildeAndAngleAreEscaped(): void {
  assertEquals(
    tiptapToMarkdown(doc(para("a ~~b~~ <u>c</u>"))),
    "a \\~\\~b\\~\\~ \\<u>c\\</u>\n",
    "literal ~~ and < must not become strikethrough or HTML",
  );
}

function testLeadingTokenEscaping(): void {
  assertEquals(
    tiptapToMarkdown(doc(para("# not a heading"))),
    "\\# not a heading\n",
    "leading hash is escaped",
  );
  assertEquals(
    tiptapToMarkdown(doc(para("1. not a list"))),
    "1\\. not a list\n",
    "leading ordered-list marker is escaped",
  );
}

function testBlockEscapesAreAnchoredToLinesNotNodes(): void {
  assertEquals(
    tiptapToMarkdown(doc({
      type: "paragraph",
      content: [
        { type: "text", text: "x" },
        { type: "text", text: "- y" },
      ],
    })),
    "x- y\n",
    "a text node in mid-line must not be treated as the start of a block",
  );
}

// -------------------------------------------------------------- code blocks

function testCodeBlock(): void {
  assertEquals(
    tiptapToMarkdown(doc(codeBlock("const a = 1;", "ts"))),
    "```ts\nconst a = 1;\n```\n",
    "fenced code block with language",
  );
}

function testBlankLinesInsideCodeBlockSurvive(): void {
  assertEquals(
    tiptapToMarkdown(doc(codeBlock("a\n\n\nb"))),
    "```\na\n\n\nb\n```\n",
    "code block content must be reproduced byte for byte",
  );
}

function testNestedFenceWidensTheFence(): void {
  assertEquals(
    tiptapToMarkdown(doc(codeBlock("```\nnested\n```"))),
    "````\n```\nnested\n```\n````\n",
    "a fence inside the body must not close the block",
  );
  assertEquals(
    tiptapToMarkdown(doc(codeBlock("a\n````\nb"))),
    "`````\na\n````\nb\n`````\n",
    "fence width is the longest run plus one",
  );
}

// ----------------------------------------------------------- whole notes

function testNoteToMarkdownAddsTitle(): void {
  assertEquals(
    noteToMarkdown({ title: "My Note", content: JSON.stringify(doc(para("body"))) }),
    "# My Note\n\nbody\n",
    "title becomes an H1",
  );
}

function testNoteToMarkdownHandlesPlainTextContent(): void {
  assertEquals(
    noteToMarkdown({ title: "T", content: "line one\n\nline two" }),
    "# T\n\nline one\n\nline two\n",
    "plain-text content (the default welcome note) is migrated, not parsed",
  );
}

function testNoteToMarkdownWithoutTitle(): void {
  assertEquals(
    noteToMarkdown({ title: "   ", content: JSON.stringify(doc(para("body"))) }),
    "body\n",
    "blank title emits no heading",
  );
}

function testNoteToMarkdownEmptyContent(): void {
  assertEquals(
    noteToMarkdown({ title: "T", content: "" }),
    "# T\n",
    "empty content yields title only",
  );
}

function testNoteTitleIsEscapedAndFlattened(): void {
  assertEquals(
    noteToMarkdown({ title: "# Hi *now*", content: JSON.stringify(doc(para("x"))) }),
    "# \\# Hi \\*now\\*\n\nx\n",
    "a title containing markdown syntax is escaped",
  );
  assertEquals(
    noteToMarkdown({ title: "line1\nline2", content: JSON.stringify(doc(para("x"))) }),
    "# line1 line2\n\nx\n",
    "a newline in the title must not split the H1",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "empty doc", fn: testEmptyDoc },
    { name: "paragraphs", fn: testParagraphs },
    { name: "headings", fn: testHeadings },
    { name: "bullet list", fn: testBulletList },
    { name: "nested list", fn: testNestedList },
    { name: "empty list item", fn: testEmptyListItemHasNoTrailingSpace },
    { name: "ordered list start", fn: testOrderedListRespectsStart },
    { name: "blockquote", fn: testBlockquote },
    { name: "hr and hard break", fn: testHorizontalRuleAndHardBreak },
    { name: "dangling hard break", fn: testDanglingHardBreakIsTrimmed },
    { name: "image", fn: testImage },
    { name: "external image src", fn: testExternalImageSrcIsPreserved },
    { name: "textAlign dropped", fn: testTextAlignIsDropped },
    { name: "marks", fn: testMarks },
    { name: "inline code is not escaped", fn: testInlineCodeIsNotEscaped },
    { name: "text escaping", fn: testTextEscaping },
    { name: "tilde and angle escaped", fn: testTildeAndAngleAreEscaped },
    { name: "leading token escaping", fn: testLeadingTokenEscaping },
    { name: "block escapes anchored to lines", fn: testBlockEscapesAreAnchoredToLinesNotNodes },
    { name: "code block", fn: testCodeBlock },
    { name: "blank lines in code block", fn: testBlankLinesInsideCodeBlockSurvive },
    { name: "nested fence", fn: testNestedFenceWidensTheFence },
    { name: "noteToMarkdown title", fn: testNoteToMarkdownAddsTitle },
    { name: "noteToMarkdown plain text", fn: testNoteToMarkdownHandlesPlainTextContent },
    { name: "noteToMarkdown blank title", fn: testNoteToMarkdownWithoutTitle },
    { name: "noteToMarkdown empty content", fn: testNoteToMarkdownEmptyContent },
    { name: "note title escaped", fn: testNoteTitleIsEscapedAndFlattened },
  ];

  await runTests("exportMarkdown.test.ts", tests);
}
