/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { addToTree } from "./addToTree";
import { collapseTree } from "./collapseTree";
import { createParentMap } from "./utils";
import { difference } from "lodash";
import { getDomain } from "./treeOrder";
import type { SourcesMap } from "../../reducers/types";
import type { TreeDirectory } from "./types";

function newSourcesSet(newSources, prevSources) {
  const newSourceIds = difference(
    Object.keys(newSources),
    Object.keys(prevSources)
  );
  const uniqSources = newSourceIds.map(id => newSources[id]);
  return uniqSources;
}

type Params = {
  newSources: SourcesMap,
  prevSources: SourcesMap,
  uncollapsedTree: TreeDirectory,
  sourceTree: TreeDirectory,
  debuggeeUrl: string,
  projectRoot: string
};

export function updateTree({
  newSources,
  prevSources,
  debuggeeUrl,
  projectRoot,
  uncollapsedTree,
  sourceTree
}: Params) {
  const newSet = newSourcesSet(newSources, prevSources);
  const debuggeeHost = getDomain(debuggeeUrl);

  for (const source of newSet) {
    addToTree(uncollapsedTree, source, debuggeeHost, projectRoot);
  }

  const newSourceTree = collapseTree(uncollapsedTree);

  return {
    uncollapsedTree,
    sourceTree: newSourceTree,
    parentMap: createParentMap(newSourceTree)
  };
}
