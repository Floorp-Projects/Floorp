/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { Node, TraversalAncestors } from "@babel/types";
export type { Node, TraversalAncestors };

export default function createSimplePath(ancestors: TraversalAncestors) {
  if (ancestors.length === 0) {
    return null;
  }

  // Slice the array because babel-types traverse may continue mutating
  // the ancestors array in later traversal logic.
  return new SimplePath(ancestors.slice());
}

export type { SimplePath };

/**
 * Mimics @babel/traverse's NodePath API in a simpler fashion that isn't as
 * heavy, but still allows the ease of passing paths around to process nested
 * AST structures.
 */
class SimplePath {
  _index: number;
  _ancestors: TraversalAncestors;
  _ancestor: $ElementType<TraversalAncestors, number>;

  _parentPath: SimplePath | null | void;

  constructor(
    ancestors: TraversalAncestors,
    index: number = ancestors.length - 1
  ) {
    if (index < 0 || index >= ancestors.length) {
      console.error(ancestors);
      throw new Error("Created invalid path");
    }

    this._ancestors = ancestors;
    this._ancestor = ancestors[index];
    this._index = index;
  }

  get parentPath(): SimplePath | null {
    let path = this._parentPath;
    if (path === undefined) {
      if (this._index === 0) {
        path = null;
      } else {
        path = new SimplePath(this._ancestors, this._index - 1);
      }
      this._parentPath = path;
    }

    return path;
  }

  get parent(): Node {
    return this._ancestor.node;
  }

  get node(): Node {
    const { node, key, index } = this._ancestor;

    if (typeof index === "number") {
      return node[key][index];
    }

    return node[key];
  }

  get key(): string {
    return this._ancestor.key;
  }

  set node(replacement: Node): void {
    if (this.type !== "Identifier") {
      throw new Error(
        "Replacing anything other than leaf nodes is undefined behavior " +
          "in t.traverse()"
      );
    }

    const { node, key, index } = this._ancestor;
    if (typeof index === "number") {
      node[key][index] = replacement;
    } else {
      node[key] = replacement;
    }
  }

  get type(): string {
    return this.node.type;
  }

  get inList(): boolean {
    return typeof this._ancestor.index === "number";
  }

  get containerIndex(): number {
    const { index } = this._ancestor;

    if (typeof index !== "number") {
      throw new Error("Cannot get index of non-array node");
    }

    return index;
  }

  get depth(): number {
    return this._index;
  }

  replace(node: Node) {
    this.node = node;
  }

  find(predicate: SimplePath => boolean): SimplePath | null {
    for (let path = this; path; path = path.parentPath) {
      if (predicate(path)) {
        return path;
      }
    }
    return null;
  }

  findParent(predicate: SimplePath => boolean): SimplePath | null {
    if (!this.parentPath) {
      throw new Error("Cannot use findParent on root path");
    }

    return this.parentPath.find(predicate);
  }

  getSibling(offset: number): ?SimplePath {
    const { node, key, index } = this._ancestor;

    if (typeof index !== "number") {
      throw new Error("Non-array nodes do not have siblings");
    }

    const container = node[key];

    const siblingIndex = index + offset;
    if (siblingIndex < 0 || siblingIndex >= container.length) {
      return null;
    }

    return new SimplePath(
      this._ancestors.slice(0, -1).concat([{ node, key, index: siblingIndex }])
    );
  }
}
