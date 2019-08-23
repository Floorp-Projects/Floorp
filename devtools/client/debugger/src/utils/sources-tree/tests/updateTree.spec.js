/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { makeMockSource } from "../../../utils/test-mockup";
import { updateTree, createTree } from "../index";

type RawSource = {| url: string, id: string, actors?: any |};

function createSourcesMap(sources: RawSource[]) {
  const sourcesMap = sources.reduce((map, source) => {
    map[source.id] = makeMockSource(source.url, source.id);
    return map;
  }, {});

  return { FakeThread: sourcesMap };
}

function formatTree(tree) {
  return JSON.stringify(tree.uncollapsedTree, null, 2);
}

const sources = [
  {
    id: "server1.conn13.child1/39",
    url: "https://davidwalsh.name/",
  },
  {
    id: "server1.conn13.child1/37",
    url: "https://davidwalsh.name/source1.js",
  },
  {
    id: "server1.conn13.child1/40",
    url: "https://davidwalsh.name/source2.js",
  },
];

const threads = [
  {
    actor: "FakeThread",
    url: "https://davidwalsh.name",
    type: "worker",
    name: "FakeThread",
  },
];

const debuggeeUrl = "blah";

describe("calls updateTree.js", () => {
  it("adds one source", () => {
    const prevSources = createSourcesMap([sources[0]]);
    const { sourceTree, uncollapsedTree } = createTree({
      debuggeeUrl,
      sources: prevSources,
      threads,
    });
    const newTree = updateTree({
      debuggeeUrl,
      prevSources,
      newSources: createSourcesMap([sources[0], sources[1]]),
      uncollapsedTree,
      sourceTree,
      threads,
    });

    expect(formatTree(newTree)).toMatchSnapshot();
  });

  it("adds two sources", () => {
    const prevSources = createSourcesMap([sources[0]]);

    const { sourceTree, uncollapsedTree } = createTree({
      debuggeeUrl,
      sources: prevSources,
      threads,
    });

    const newTree = updateTree({
      debuggeeUrl,
      prevSources,
      newSources: createSourcesMap([sources[0], sources[1], sources[2]]),
      uncollapsedTree,
      sourceTree,
      projectRoot: "",
      threads,
    });

    expect(formatTree(newTree)).toMatchSnapshot();
  });

  // NOTE: we currently only add sources to the tree and clear the tree
  // on navigate.
  it("shows all the sources", () => {
    const prevSources = createSourcesMap([sources[0]]);

    const { sourceTree, uncollapsedTree } = createTree({
      debuggeeUrl,
      sources: prevSources,
      threads,
    });

    const newTree = updateTree({
      debuggeeUrl,
      prevSources,
      newSources: createSourcesMap([sources[1]]),
      uncollapsedTree,
      sourceTree,
      projectRoot: "",
      threads,
    });

    expect(formatTree(newTree)).toMatchSnapshot();
  });
});
