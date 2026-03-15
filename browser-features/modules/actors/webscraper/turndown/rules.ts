/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Rule management for Turndown
 * Based on https://github.com/mixmark-io/turndown
 * Copyright (c) 2014 Dom Christie
 * Released under the MIT License
 */

import type { ExtendedNode } from "./node";

export interface Rule {
  filter: string | string[] | ((node: ExtendedNode, options?: TurndownOptions) => boolean);
  replacement: (
    content: string,
    node: ExtendedNode,
    options: TurndownOptions,
  ) => string;
  append?: (options: TurndownOptions) => string;
  [key: string]: unknown; // Allow additional properties like 'references'
}

export interface TurndownOptions {
  rules: Record<string, Rule>;
  headingStyle: "setext" | "atx";
  hr: string;
  bulletListMarker: string;
  codeBlockStyle: "indented" | "fenced";
  fence: string;
  emDelimiter: string;
  strongDelimiter: string;
  linkStyle: "inlined" | "referenced";
  linkReferenceStyle: "full" | "collapsed" | "shortcut";
  br: string;
  preformattedCode: boolean;
  blankReplacement: (content: string, node: ExtendedNode) => string;
  keepReplacement: (content: string, node: ExtendedNode) => string;
  defaultReplacement: (content: string, node: ExtendedNode) => string;
}

interface ReplacementRule {
  filter?: string | string[] | ((node: ExtendedNode, options?: TurndownOptions) => boolean);
  replacement: (content: string, node: ExtendedNode, options: TurndownOptions) => string;
  append?: (options: TurndownOptions) => string;
}

export default class Rules {
  options: TurndownOptions;
  private _keep: ReplacementRule[] = [];
  private _remove: ReplacementRule[] = [];
  blankRule: ReplacementRule;
  keepReplacement: (content: string, node: ExtendedNode) => string;
  defaultRule: ReplacementRule;
  array: Rule[] = [];

  constructor(options: TurndownOptions) {
    this.options = options;
    this._keep = [];
    this._remove = [];

    this.blankRule = {
      replacement: options.blankReplacement,
    };

    this.keepReplacement = options.keepReplacement;

    this.defaultRule = {
      replacement: options.defaultReplacement,
    };

    this.array = [];
    for (const key in options.rules) {
      this.array.push(options.rules[key] as Rule);
    }
  }

  add(key: string, rule: Rule): void {
    this.array.unshift(rule);
  }

  keep(filter: string | string[]): void {
    this._keep.unshift({
      filter: filter,
      replacement: this.keepReplacement,
    });
  }

  remove(filter: string | string[]): void {
    this._remove.unshift({
      filter: filter,
      replacement: function () {
        return "";
      },
    });
  }

  forNode(node: ExtendedNode): ReplacementRule {
    if (node.isBlank) {
      return this.blankRule;
    }

    let rule = findRule(this.array, node, this.options);
    if (rule) {
      return rule;
    }

    rule = findRule(this._keep, node, this.options);
    if (rule) {
      return rule;
    }

    rule = findRule(this._remove, node, this.options);
    if (rule) {
      return rule;
    }

    return this.defaultRule;
  }

  forEach(fn: (rule: Rule, index: number) => void): void {
    for (let i = 0; i < this.array.length; i++) {
      fn(this.array[i], i);
    }
  }
}

function findRule(
  rules: ReplacementRule[],
  node: ExtendedNode,
  options: TurndownOptions,
): ReplacementRule | undefined {
  for (let i = 0; i < rules.length; i++) {
    const rule = rules[i];
    if (filterValue(rule, node, options)) {
      return rule;
    }
  }
  return undefined;
}

function filterValue(
  rule: ReplacementRule,
  node: ExtendedNode,
  options: TurndownOptions,
): boolean {
  const filter = rule.filter;
  if (!filter) {
    return false;
  }
  if (typeof filter === "string") {
    return filter === node.nodeName.toLowerCase();
  } else if (Array.isArray(filter)) {
    return filter.indexOf(node.nodeName.toLowerCase()) > -1;
  } else if (typeof filter === "function") {
    return filter.call(rule, node, options);
  } else {
    throw new TypeError("`filter` needs to be a string, array, or function");
  }
}
