// SPDX-License-Identifier: MPL-2.0

import type { JSONContent } from "@tiptap/react";
import { migrateLexicalContent } from "./migrateLexicalToTiptap.ts";
import type { Note } from "../types/note.ts";

/**
 * Serializes Floorp Notes content to Markdown.
 *
 * The node set is fixed by the editor configuration in
 * `components/editor/RichTextEditor.tsx` (StarterKit + Underline + TextAlign +
 * Image). There is no `link` extension, so no link handling exists here.
 *
 * `underline` has no Markdown equivalent and is emitted as inline HTML;
 * `textAlign` has none either and is dropped.
 */

const MARK_WRAPPERS: Record<string, readonly [string, string]> = {
  bold: ["**", "**"],
  italic: ["*", "*"],
  strike: ["~~", "~~"],
  underline: ["<u>", "</u>"],
};

/**
 * Escapes characters that would otherwise be read as Markdown syntax.
 *
 * `~` and `<` are included so that a note containing a literal `~~text~~` or
 * `<u>` does not silently render as strikethrough or underline. The
 * Hashes are escaped even mid-line because they can close an ATX heading.
 * Ampersands must not turn literal entity references into different text.
 * The block-leading rules apply only when this text actually begins a line.
 */
function escapeText(text: string, atLineStart: boolean): string {
  const out = text.replace(/&/g, "&amp;").replace(/([\\`*_[\]~<#])/g, "\\$1");
  if (!atLineStart) {
    return out;
  }
  return out
    .replace(/^(\s*)(>|[-+])/, "$1\\$2")
    .replace(/^(\s*)(\d+)([.)])/, "$1$2\\$3");
}

/** Wraps text in backticks, widening the fence if the text contains one. */
function renderCode(raw: string): string {
  const longestRun = [...raw.matchAll(/`+/g)].reduce(
    (longest, match) => Math.max(longest, match[0].length),
    0,
  );
  const fence = "`".repeat(longestRun + 1);
  // CommonMark strips one surrounding space when the span is not all spaces.
  const needsPadding = raw.startsWith("`") || raw.endsWith("`") ||
    (raw.startsWith(" ") && raw.endsWith(" ") && /[^ ]/.test(raw));
  const pad = needsPadding ? " " : "";
  return `${fence}${pad}${raw}${pad}${fence}`;
}

function renderTextNode(node: JSONContent, atLineStart: boolean): string {
  const raw = node.text ?? "";
  const marks = node.marks ?? [];
  const isCode = marks.some((mark) => mark.type === "code");

  let out = isCode ? renderCode(raw) : escapeText(raw, atLineStart);
  for (const mark of marks) {
    const wrapper = MARK_WRAPPERS[mark.type];
    if (wrapper) {
      out = `${wrapper[0]}${out}${wrapper[1]}`;
    }
  }
  return out;
}

function renderImage(node: JSONContent): string {
  // Preserve data URIs and existing percent escapes. Angle-delimited
  // destinations allow spaces and unbalanced parentheses; line endings and
  // angle brackets still need encoding, and backslashes need escaping.
  const src = typeof node.attrs?.src === "string" ? node.attrs.src : "";
  if (!src) {
    return "";
  }
  const rawAlt = typeof node.attrs?.alt === "string" ? node.attrs.alt : "";
  const alt = escapeText(rawAlt.replace(/[\r\n]+/g, " "), false);
  const escapedSrc = src.replace(/[<>\r\n]/g, encodeURIComponent)
    .replace(/\\/g, "\\\\").replace(/&/g, "&amp;");
  const destination = /[\s()<>]/.test(src) ? `<${escapedSrc}>` : escapedSrc;
  return `![${alt}](${destination})`;
}

function renderInline(nodes: JSONContent[] | undefined): string {
  if (!nodes) {
    return "";
  }
  const parts: string[] = [];
  // A paragraph is often several text nodes because of mark boundaries, so
  // line-start is tracked across the loop rather than taken from the index.
  let atLineStart = true;
  for (const node of nodes) {
    switch (node.type) {
      case "text":
        parts.push(renderTextNode(node, atLineStart));
        atLineStart = false;
        break;
      case "hardBreak":
        parts.push("  \n");
        atLineStart = true;
        break;
      case "image":
        parts.push(renderImage(node));
        atLineStart = false;
        break;
      default:
        parts.push(renderInline(node.content));
        atLineStart = false;
        break;
    }
  }
  return parts.join("");
}

function prefixLines(text: string, prefix: string): string {
  return text
    .split("\n")
    .map((line) => (line.length > 0 ? prefix + line : prefix.trimEnd()))
    .join("\n");
}

function renderList(
  list: JSONContent,
  marker: (index: number) => string,
): string {
  const items = list.content ?? [];
  return items
    .map((item, index) => {
      const bullet = marker(index);
      const indent = " ".repeat(bullet.length);
      const [first = "", ...rest] = renderBlocks(item.content).split("\n");
      const head = first.length > 0 ? bullet + first : bullet.trimEnd();
      return [
        head,
        ...rest.map((line) => (line.length > 0 ? indent + line : line)),
      ].join("\n");
    })
    .join("\n");
}

function renderBlock(node: JSONContent): string {
  switch (node.type) {
    case "paragraph":
      return renderInline(node.content);
    case "heading": {
      const raw = Number(node.attrs?.level ?? 1);
      const level = Math.min(Math.max(Number.isFinite(raw) ? raw : 1, 1), 6);
      return `${"#".repeat(level)} ${renderInline(node.content)}`;
    }
    case "codeBlock": {
      const language = typeof node.attrs?.language === "string"
        ? node.attrs.language
        : "";
      const body = (node.content ?? [])
        .map((child) => child.text ?? "")
        .join("");
      // The fence must be longer than the longest backtick run in the
      // body, or a note containing ``` closes its own code block early.
      const runs = [...body.matchAll(/`+/g)].map((match) => match[0].length);
      const fence = "`".repeat(Math.max(3, Math.max(0, ...runs) + 1));
      return `${fence}${language}\n${body}\n${fence}`;
    }
    case "blockquote":
      return prefixLines(renderBlocks(node.content), "> ");
    case "bulletList":
      return renderList(node, () => "- ");
    case "orderedList": {
      const raw = Number(node.attrs?.start ?? 1);
      const start = Number.isFinite(raw) ? raw : 1;
      return renderList(node, (index) => `${start + index}. `);
    }
    case "horizontalRule":
      return "---";
    case "image":
      return renderImage(node);
    default:
      return renderBlocks(node.content);
  }
}

function renderBlocks(nodes: JSONContent[] | undefined): string {
  if (!nodes) {
    return "";
  }
  // Trailing whitespace is trimmed per block and never document-wide: a
  // document-wide newline collapse would also rewrite code block contents.
  return nodes
    .map(renderBlock)
    .map((block) => block.replace(/\s+$/, ""))
    .filter((block) => block.length > 0)
    .join("\n\n");
}

/** Serializes a TipTap document to Markdown. Returns "" for an empty document. */
export function tiptapToMarkdown(doc: JSONContent | undefined): string {
  if (!doc || doc.type !== "doc") {
    return "";
  }
  const body = renderBlocks(doc.content);
  return body.length > 0 ? `${body}\n` : "";
}

/**
 * Serializes a whole note, title included, to a Markdown document.
 *
 * `note.content` may be TipTap JSON, legacy Lexical JSON, or plain text — the
 * default welcome note is plain text — so it is always normalised through
 * `migrateLexicalContent` rather than parsed directly.
 */
export function noteToMarkdown(note: Pick<Note, "title" | "content">): string {
  const body = tiptapToMarkdown(migrateLexicalContent(note.content));
  // The title reaches an H1, so it is escaped and flattened to one line. It
  // normally comes from an <input>, but it can also arrive from the iOS/sync
  // wire format (ADR-001), which imposes no such restriction.
  const heading = escapeText(note.title.replace(/\s+/g, " ").trim(), true);
  if (!heading) {
    return body;
  }
  return body.length > 0 ? `# ${heading}\n\n${body}` : `# ${heading}\n`;
}
