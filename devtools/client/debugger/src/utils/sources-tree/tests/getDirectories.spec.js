/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { makeMockDisplaySource } from "../../../utils/test-mockup";

import { getDirectories, findSourceTreeNodes, createTree } from "../index";

function formatDirectories(source, tree) {
  const paths = getDirectories(source, tree);
  return paths.map(node => node.path);
}

function createSources(urls) {
  return {
    FakeThread: urls.reduce((sources, url, index) => {
      const id = `a${index}`;
      sources[id] = makeMockDisplaySource(url, id);
      return sources;
    }, {}),
  };
}

describe("getDirectories", () => {
  it("gets a source's ancestor directories", function() {
    const sources = createSources([
      "http://a/b.js",
      "http://a/c.js",
      "http://b/c.js",
    ]);

    const threads = [
      {
        actor: "FakeThread",
        url: "http://a",
        targetType: "worker",
        name: "FakeThread",
        isTopLevel: false,
      },
    ];

    const mainThreadHost = "a";
    const { sourceTree } = createTree({
      sources,
      mainThreadHost,
      threads,
    });

    expect(formatDirectories(sources.FakeThread.a0, sourceTree)).toEqual([
      "FakeThread/a/b.js",
      "FakeThread/a",
      "FakeThread",
    ]);
    expect(formatDirectories(sources.FakeThread.a1, sourceTree)).toEqual([
      "FakeThread/a/c.js",
      "FakeThread/a",
      "FakeThread",
    ]);
    expect(formatDirectories(sources.FakeThread.a2, sourceTree)).toEqual([
      "FakeThread/b/c.js",
      "FakeThread/b",
      "FakeThread",
    ]);
  });
});

describe("findSourceTreeNodes", () => {
  it("finds a node", () => {
    const sources = createSources([
      "http://src/main.js",
      "http://src/utils/help.js",
      "http://src/utils/print.js",
      "http://workers/worker.js",
    ]);

    const threads = [
      {
        actor: "FakeThread",
        url: "http://a",
        targetType: "worker",
        name: "FakeThread",
        isTopLevel: false,
      },
    ];

    const mainThreadHost = "a";
    const { sourceTree } = createTree({
      sources,
      mainThreadHost,
      threads,
    });

    const nodes = findSourceTreeNodes(sourceTree, "src") || [];
    expect(nodes[0].path).toEqual("FakeThread/src");
  });
});
