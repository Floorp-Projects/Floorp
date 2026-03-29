/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Turndown - HTML to Markdown converter
 * Customized for Firefox actor environment
 *
 * Based on https://github.com/mixmark-io/turndown
 * Copyright (c) 2014 Dom Christie
 * Released under the MIT License
 * https://github.com/mixmark-io/turndown/blob/master/LICENSE
 */

import COMMONMARK_RULES from "./commonmark-rules";
import Rules, { type TurndownOptions, type Rule } from "./rules";
import type { ExtendedNode } from "./node";
import { extend } from "./utilities";
import RootNode from "./root-node";
import wrapNode from "./node";
import {
  formatFingerprintComment,
  formatSelectorMapEntry,
  FingerprintTextCache,
  type ElementFingerprint,
} from "./fingerprint";

// Global (position-independent) escape characters — single regex + map
const globalEscapeRe = /[\\*`\[\]_]/g;
const globalEscapeMap: Record<string, string> = {
  "\\": "\\\\",
  "*": "\\*",
  "`": "\\`",
  "[": "\\[",
  "]": "\\]",
  "_": "\\_",
};

// Line-start (anchored) patterns — single regex with alternation
const lineStartEscapeRe = /^(?:(-)|(\+ )|(=+)|(#{1,6}) |(~~~)|(>)|(\d+)\. )/;

export interface TurndownServiceOptions {
  headingStyle?: "setext" | "atx";
  hr?: string;
  bulletListMarker?: string;
  codeBlockStyle?: "indented" | "fenced";
  fence?: string;
  emDelimiter?: string;
  strongDelimiter?: string;
  linkStyle?: "inlined" | "referenced";
  linkReferenceStyle?: "full" | "collapsed" | "shortcut";
  br?: string;
  preformattedCode?: boolean;
  /** Enable element fingerprinting in output (default: false) */
  enableFingerprints?: boolean;
  /** Append selector map at the end of output (default: false) */
  fingerprintSelectorMap?: boolean;
}

export class TurndownService {
  options: TurndownOptions;
  rules: Rules;
  /** Collected fingerprints for selector map generation */
  collectedFingerprints: Array<{
    fingerprint: ElementFingerprint;
    tagName: string;
    textPreview: string;
  }> = [];
  /** Pre-computed text cache for O(1) fingerprint text lookups */
  textCache: FingerprintTextCache | undefined;

  constructor(options: TurndownServiceOptions = {}) {
    const defaults: TurndownOptions = {
      rules: COMMONMARK_RULES,
      headingStyle: "setext",
      hr: "* * *",
      bulletListMarker: "*",
      codeBlockStyle: "indented",
      fence: "```",
      emDelimiter: "_",
      strongDelimiter: "**",
      linkStyle: "inlined",
      linkReferenceStyle: "full",
      br: "  ",
      preformattedCode: false,
      enableFingerprints: false,
      fingerprintSelectorMap: false,
      blankReplacement: function (content: string, node: ExtendedNode): string {
        return node.isBlock ? "\n\n" : "";
      },
      keepReplacement: function (content: string, node: ExtendedNode): string {
        const html = (node as unknown as Element).outerHTML as unknown as string;
        return node.isBlock ? "\n\n" + html + "\n\n" : html;
      },
      defaultReplacement: function (
        content: string,
        node: ExtendedNode,
      ): string {
        return node.isBlock ? "\n\n" + content + "\n\n" : content;
      },
    };

    // Merge options with defaults
    this.options = {
      ...defaults,
      ...options,
    } as TurndownOptions;
    this.rules = new Rules(this.options);
  }

  /**
   * The entry point for converting a string or DOM node to Markdown
   * @param input The string or DOM node to convert
   * @returns A Markdown representation of the input
   */
  turndown(input: string | Element, opts?: { skipClone?: boolean }): string {
    // Reset collected fingerprints for each conversion
    this.collectedFingerprints = [];

    if (!canConvert(input)) {
      throw new TypeError(
        input + " is not a string, or an element/document/fragment node.",
      );
    }

    if (input === "") {
      return "";
    }

    // RootNode clones the input element (and applies collapseWhitespace),
    // so the textCache must be built on the clone that process() will
    // actually traverse — otherwise the WeakMap keys won't match.
    const rootNode = RootNode(input, {
      ...this.options,
      skipClone: opts?.skipClone,
    });
    if (this.options.enableFingerprints && typeof input !== "string") {
      this.textCache = new FingerprintTextCache();
      this.textCache.precompute(rootNode);
    } else {
      this.textCache = undefined;
    }

    const output = process.call(this, rootNode as unknown as Node);
    let result = postProcess.call(this, output);

    // Remove isolated fingerprint lines — lines that contain only fingerprint
    // comment(s) with no other text. These occur when a parent block element
    // has child text (passing the node.ts check) but its own Markdown output
    // is empty because children render in their own blocks.
    if (this.options.enableFingerprints) {
      result = result.replace(/^[ \t]*(?:<!--fp:[a-z0-9]+-->[ \t]*)+$/gm, "");
      // Collapse 3+ consecutive blank lines to 2
      result = result.replace(/\n{3,}/g, "\n\n");
    }

    // Append selector map if enabled and fingerprints were collected
    if (
      this.options.fingerprintSelectorMap &&
      this.collectedFingerprints.length > 0
    ) {
      result += "\n\n---\n\n#### Element Selector Map\n\n";
      result +=
        "<!-- Fingerprint -> Element mappings for programmatic access -->\n";
      for (const entry of this.collectedFingerprints) {
        result +=
          formatSelectorMapEntry(
            entry.fingerprint,
            entry.tagName,
            entry.textPreview,
          ) + "\n";
      }
    }

    return result;
  }

  /**
   * Add one or more plugins
   * @param plugin The plugin or array of plugins to add
   * @returns The Turndown instance for chaining
   */
  use(
    plugin:
      | ((service: TurndownService) => void)
      | ((service: TurndownService) => void)[],
  ): TurndownService {
    if (Array.isArray(plugin)) {
      for (let i = 0; i < plugin.length; i++) {
        this.use(plugin[i]);
      }
    } else if (typeof plugin === "function") {
      plugin(this);
    } else {
      throw new TypeError("plugin must be a Function or an Array of Functions");
    }
    return this;
  }

  /**
   * Adds a rule
   * @param key The unique key of the rule
   * @param rule The rule
   * @returns The Turndown instance for chaining
   */
  addRule(key: string, rule: Partial<Rule>): TurndownService {
    this.rules.add(key, rule as Rule);
    return this;
  }

  /**
   * Keep a node (as HTML) that matches the filter
   * @param filter The filter to match
   * @returns The Turndown instance for chaining
   */
  keep(filter: string | string[]): TurndownService {
    this.rules.keep(filter);
    return this;
  }

  /**
   * Remove a node that matches the filter
   * @param filter The filter to match
   * @returns The Turndown instance for chaining
   */
  remove(filter: string | string[]): TurndownService {
    this.rules.remove(filter);
    return this;
  }

  /**
   * Escapes Markdown syntax
   * @param string The string to escape
   * @returns A string with Markdown syntax escaped
   */
  escape(string: string): string {
    // Global patterns first — so backslashes inserted by line-start escaping
    // below are not re-escaped.
    string = string.replace(globalEscapeRe, (ch) => globalEscapeMap[ch]);
    // Line-start patterns (single regex, replaces 7 anchored patterns)
    return string.replace(
      lineStartEscapeRe,
      (_match, dash, plus, equals, hash, tilde, gt, digits) => {
        if (dash) return "\\-";
        if (plus) return "\\+ ";
        if (equals) return "\\" + equals;
        if (hash) return "\\" + hash + " ";
        if (tilde) return "\\~~~";
        if (gt) return "\\>";
        if (digits) return digits + "\\. ";
        return _match;
      },
    );
  }
}

/**
 * Reduces a DOM node down to its Markdown string equivalent
 */
function process(this: TurndownService, parentNode: Node): string {
  const self = this;
  const childNodes = parentNode.childNodes
    ? (Array.from(parentNode.childNodes) as Array<Node | null>).filter(
        (n): n is Node => n !== null,
      )
    : [];

  return childNodes.reduce(function (output: string, node: Node): string {
    const wrappedNode = wrapNode(node, self.options, self.textCache);

    let replacement = "";
    if (wrappedNode.nodeType === 3) {
      // Node.TEXT_NODE
      replacement = wrappedNode.isCode
        ? wrappedNode.nodeValue || ""
        : self.escape(wrappedNode.nodeValue || "");
    } else if (wrappedNode.nodeType === 1) {
      // Node.ELEMENT_NODE
      replacement = replacementForNode.call(self, wrappedNode);
    }

    return join(output, replacement);
  }, "");
}

/**
 * Appends strings as each rule requires and trims the output
 */
function postProcess(this: TurndownService, output: string): string {
  const self = this;
  self.rules.forEach(function (rule) {
    if (typeof rule.append === "function") {
      output = join(output, rule.append(self.options));
    }
  });

  return output.replace(/^[\t\r\n]+/, "").replace(/[\t\r\n\s]+$/, "");
}

/**
 * Converts an element node to its Markdown equivalent
 */
function replacementForNode(this: TurndownService, node: ExtendedNode): string {
  const self = this;
  const rule = this.rules.forNode(node);
  let content = process.call(this, node as unknown as Node);
  const whitespace = node.flankingWhitespace;
  if (whitespace.leading || whitespace.trailing) {
    content = content.trim();
  }

  let replacement = rule.replacement(content, node, this.options);

  // Embed fingerprint as HTML comment for block elements when enabled
  if (this.options.enableFingerprints && node.fingerprint && node.isBlock) {
    const fpComment = formatFingerprintComment(node.fingerprint);

    // Handle list markers specially - insert fingerprint after the marker
    // to avoid breaking Markdown list syntax (e.g., "- item" -> "- <!--fp:...-->item")
    const listMarkerMatch = replacement.match(/^(\s*)([-*+]|\d+\.)(\s+)/);
    if (listMarkerMatch) {
      replacement =
        listMarkerMatch[1] +
        listMarkerMatch[2] +
        listMarkerMatch[3] +
        fpComment +
        replacement.slice(listMarkerMatch[0].length);
    } else {
      // Insert fingerprint at the first non-whitespace position so it stays
      // on the same line as content instead of becoming an isolated line.
      const firstNonWS = replacement.search(/\S/);
      if (firstNonWS > 0) {
        replacement =
          replacement.slice(0, firstNonWS) +
          fpComment +
          replacement.slice(firstNonWS);
      } else {
        replacement = fpComment + replacement;
      }
    }

    // Collect fingerprint for selector map
    if (this.options.fingerprintSelectorMap) {
      self.collectedFingerprints.push({
        fingerprint: node.fingerprint,
        tagName: node.nodeName.toLowerCase(),
        textPreview: (node.textContent || "").slice(0, 50),
      });
    }
  }

  return whitespace.leading + replacement + whitespace.trailing;
}

/**
 * Joins replacement to the current output with appropriate number of new lines
 */
function join(output: string, replacement: string): string {
  // Count trailing newlines in output without creating a substring
  let trailingNLs = 0;
  for (let i = output.length - 1; i >= 0 && output[i] === "\n"; i--) {
    trailingNLs++;
  }
  // Count leading newlines in replacement without creating a substring
  let leadingNLs = 0;
  for (let j = 0; j < replacement.length && replacement[j] === "\n"; j++) {
    leadingNLs++;
  }
  const nls = Math.max(trailingNLs, leadingNLs);
  const separator = "\n\n".substring(0, nls);

  return (
    output.substring(0, output.length - trailingNLs) +
    separator +
    replacement.substring(leadingNLs)
  );
}

/**
 * Determines whether an input can be converted
 */
function canConvert(input: unknown): boolean {
  return (
    input != null &&
    (typeof input === "string" ||
      (typeof input === "object" &&
        "nodeType" in input &&
        (input.nodeType === 1 ||
          input.nodeType === 9 ||
          input.nodeType === 11)))
  );
}
