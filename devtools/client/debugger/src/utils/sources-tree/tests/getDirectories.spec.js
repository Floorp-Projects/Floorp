/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { makeMockSource } from "../../../utils/test-mockup";

import { getDirectories, createTree } from "../index";

function formatDirectories(source, tree) {
  const paths: any = getDirectories(source, tree);
  return paths.map(node => node.path);
}

function createSources(urls) {
  return {
    FakeThread: urls.reduce((sources, url, index) => {
      const id = `a${index}`;
      sources[id] = makeMockSource(url, id);
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
        type: 1,
        name: "FakeThread",
      },
    ];

    const debuggeeUrl = "http://a/";
    const { sourceTree } = createTree({
      sources,
      debuggeeUrl,
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
