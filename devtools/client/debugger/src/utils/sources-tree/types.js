/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { Source } from "../../types";

/**
 * TODO: createNode is exported so this type could be useful to other modules
 * @memberof utils/sources-tree
 * @static
 */
export type TreeNode = TreeSource | TreeDirectory;

export type TreeSource = {
  type: "source",
  name: string,
  path: string,
  contents: Source,
};

export type TreeDirectory = {
  type: "directory",
  name: string,
  path: string,
  contents: TreeNode[],
};

export type ParentMap = WeakMap<TreeNode, TreeDirectory>;

export type SourcesGroups = {
  sourcesInside: Source[],
  sourcesOuside: Source[],
};
