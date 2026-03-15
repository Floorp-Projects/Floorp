/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Utility functions for Turndown HTML to Markdown converter
 * Based on https://github.com/mixmark-io/turndown
 */

export function extend(
  destination: Record<string, unknown>,
  ...sources: Record<string, unknown>[]
): Record<string, unknown> {
  for (const source of sources) {
    for (const key in source) {
      if (Object.prototype.hasOwnProperty.call(source, key)) {
        destination[key] = source[key];
      }
    }
  }
  return destination;
}

export function repeat(character: string, count: number): string {
  return Array(count + 1).join(character);
}

export function trimLeadingNewlines(string: string): string {
  return string.replace(/^\n*/, "");
}

export function trimTrailingNewlines(string: string): string {
  // Avoid match-at-end regexp bottleneck, see #370
  let indexEnd = string.length;
  while (indexEnd > 0 && string[indexEnd - 1] === "\n") {
    indexEnd--;
  }
  return string.substring(0, indexEnd);
}

export const blockElements = [
  "ADDRESS",
  "ARTICLE",
  "ASIDE",
  "AUDIO",
  "BLOCKQUOTE",
  "BODY",
  "CANVAS",
  "CENTER",
  "DD",
  "DIR",
  "DIV",
  "DL",
  "DT",
  "FIELDSET",
  "FIGCAPTION",
  "FIGURE",
  "FOOTER",
  "FORM",
  "FRAMESET",
  "H1",
  "H2",
  "H3",
  "H4",
  "H5",
  "H6",
  "HEADER",
  "HGROUP",
  "HR",
  "HTML",
  "ISINDEX",
  "LI",
  "MAIN",
  "MENU",
  "NAV",
  "NOFRAMES",
  "NOSCRIPT",
  "OL",
  "OUTPUT",
  "P",
  "PRE",
  "SECTION",
  "TABLE",
  "TBODY",
  "TD",
  "TFOOT",
  "TH",
  "THEAD",
  "TR",
  "UL",
];

export function isBlock(node: Node): boolean {
  return is(node, blockElements);
}

export const voidElements = [
  "AREA",
  "BASE",
  "BR",
  "COL",
  "COMMAND",
  "EMBED",
  "HR",
  "IMG",
  "INPUT",
  "KEYGEN",
  "LINK",
  "META",
  "PARAM",
  "SOURCE",
  "TRACK",
  "WBR",
];

export function isVoid(node: Node): boolean {
  return is(node, voidElements);
}

export function hasVoid(node: Node): boolean {
  return has(node, voidElements);
}

const meaningfulWhenBlankElements = [
  "A",
  "TABLE",
  "THEAD",
  "TBODY",
  "TFOOT",
  "TH",
  "TD",
  "IFRAME",
  "SCRIPT",
  "AUDIO",
  "VIDEO",
];

export function isMeaningfulWhenBlank(node: Node): boolean {
  return is(node, meaningfulWhenBlankElements);
}

export function hasMeaningfulWhenBlank(node: Node): boolean {
  return has(node, meaningfulWhenBlankElements);
}

function is(node: Node, tagNames: string[]): boolean {
  return tagNames.indexOf(node.nodeName) >= 0;
}

function has(node: Node, tagNames: string[]): boolean {
  const element = node as Element;
  return (
    element.getElementsByTagName !== undefined &&
    tagNames.some(function (tagName) {
      return element.getElementsByTagName(tagName).length > 0;
    })
  );
}
