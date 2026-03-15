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
import {
  extend,
  trimLeadingNewlines,
  trimTrailingNewlines,
} from "./utilities";
import RootNode from "./root-node";
import wrapNode from "./node";

interface EscapePattern {
  pattern: RegExp;
  replacement: string;
}

const escapes: EscapePattern[] = [
  { pattern: /\\/g, replacement: "\\\\" },
  { pattern: /\*/g, replacement: "\\*" },
  { pattern: /^-/g, replacement: "\\-" },
  { pattern: /^\+ /g, replacement: "\\+ " },
  { pattern: /^(=+)/g, replacement: "\\$1" },
  { pattern: /^(#{1,6}) /g, replacement: "\\$1 " },
  { pattern: /`/g, replacement: "\\`" },
  { pattern: /^~~~/g, replacement: "\\~~~" },
  { pattern: /\[/g, replacement: "\\[" },
  { pattern: /\]/g, replacement: "\\]" },
  { pattern: /^>/g, replacement: "\\>" },
  { pattern: /_/g, replacement: "\\_" },
  { pattern: /^(\d+)\. /g, replacement: "$1\\\. " },
];

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
}

export class TurndownService {
  private options: TurndownOptions;
  rules: Rules;

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
      blankReplacement: function (content: string, node: ExtendedNode): string {
        return node.isBlock ? "\n\n" : "";
      },
      keepReplacement: function (content: string, node: ExtendedNode): string {
        return node.isBlock
          ? "\n\n" + (node as unknown as Element).outerHTML + "\n\n"
          : (node as unknown as Element).outerHTML;
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
  turndown(input: string | Element): string {
    if (!canConvert(input)) {
      throw new TypeError(
        input + " is not a string, or an element/document/fragment node.",
      );
    }

    if (input === "") {
      return "";
    }

    const output = process.call(
      this,
      RootNode(input, this.options) as unknown as ParentNode,
    );
    return postProcess.call(this, output);
  }

  /**
   * Add one or more plugins
   * @param plugin The plugin or array of plugins to add
   * @returns The Turndown instance for chaining
   */
  use(plugin: ((service: TurndownService) => void) | ((service: TurndownService) => void)[]): TurndownService {
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
    return escapes.reduce(function (accumulator, escape) {
      return accumulator.replace(escape.pattern, escape.replacement);
    }, string);
  }
}

/**
 * Reduces a DOM node down to its Markdown string equivalent
 */
function process(parentNode: ParentNode): string {
  const self = this;
  const childNodes = parentNode.childNodes
    ? Array.from(parentNode.childNodes)
    : [];

  return childNodes.reduce(function (output, node) {
    const wrappedNode = wrapNode(node, self.options);

    let replacement = "";
    if (wrappedNode.nodeType === 3) { // Node.TEXT_NODE
      replacement = wrappedNode.isCode
        ? wrappedNode.nodeValue || ""
        : self.escape(wrappedNode.nodeValue || "");
    } else if (wrappedNode.nodeType === 1) { // Node.ELEMENT_NODE
      replacement = replacementForNode.call(self, wrappedNode);
    }

    return join(output, replacement);
  }, "");
}

/**
 * Appends strings as each rule requires and trims the output
 */
function postProcess(output: string): string {
  const self = this;
  self.rules.forEach(function (rule) {
    if (typeof rule.append === "function") {
      output = join(output, rule.append(self.options));
    }
  });

  return output
    .replace(/^[\t\r\n]+/, "")
    .replace(/[\t\r\n\s]+$/, "");
}

/**
 * Converts an element node to its Markdown equivalent
 */
function replacementForNode(node: ExtendedNode): string {
  const rule = this.rules.forNode(node);
  let content = process.call(this, node as unknown as ParentNode);
  const whitespace = node.flankingWhitespace;
  if (whitespace.leading || whitespace.trailing) {
    content = content.trim();
  }
  return (
    whitespace.leading +
    rule.replacement(content, node, this.options) +
    whitespace.trailing
  );
}

/**
 * Joins replacement to the current output with appropriate number of new lines
 */
function join(output: string, replacement: string): string {
  const s1 = trimTrailingNewlines(output);
  const s2 = trimLeadingNewlines(replacement);
  const nls = Math.max(
    output.length - s1.length,
    replacement.length - s2.length,
  );
  const separator = "\n\n".substring(0, nls);

  return s1 + separator + s2;
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
        (input.nodeType === 1 || input.nodeType === 9 || input.nodeType === 11)))
  );
}
