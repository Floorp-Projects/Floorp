/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { makeMockDisplaySource } from "../../../utils/test-mockup";
import { updateTree, createTree } from "../index";

function createSourcesMap(sources) {
  const sourcesMap = sources.reduce((map, source) => {
    map[source.id] = makeMockDisplaySource(source.url, source.id);
    return map;
  }, {});

  return { FakeThread: sourcesMap };
}

function formatTree(tree) {
  if (!tree) {
    throw new Error("Tree must exist");
  }
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
    targetType: "worker",
    name: "FakeThread",
    isTopLevel: false,
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

  it("update sources that change their display URL", () => {
    const prevSources = createSourcesMap([sources[0]]);

    const { sourceTree, uncollapsedTree } = createTree({
      debuggeeUrl,
      sources: prevSources,
      threads,
    });

    const newTree = updateTree({
      debuggeeUrl,
      prevSources,
      newSources: createSourcesMap([
        {
          ...sources[0],
          url: `${sources[0].url}?param`,
        },
      ]),
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
      newSources: createSourcesMap([sources[0], sources[1]]),
      uncollapsedTree,
      sourceTree,
      projectRoot: "",
      threads,
    });

    expect(formatTree(newTree)).toMatchSnapshot();
  });
});
