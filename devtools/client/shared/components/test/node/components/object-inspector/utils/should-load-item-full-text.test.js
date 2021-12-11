/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const Utils = require("devtools/client/shared/components/object-inspector/utils/index");
const { createNode } = Utils.node;
const { shouldLoadItemFullText } = Utils.loadProperties;

const longStringStubs = require("devtools/client/shared/components/test/node/stubs/reps/long-string");
const symbolStubs = require("devtools/client/shared/components/test/node/stubs/reps/symbol");

describe("shouldLoadItemFullText", () => {
  it("returns true for a longString node with unloaded full text", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: longStringStubs.get("testUnloadedFullText"),
      },
    });
    expect(shouldLoadItemFullText(node)).toBeTruthy();
  });

  it("returns false for a longString node with loaded full text", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: longStringStubs.get("testLoadedFullText"),
      },
    });
    const loadedProperties = new Map([[node.path, true]]);
    expect(shouldLoadItemFullText(node, loadedProperties)).toBeFalsy();
  });

  it("returns false for non longString primitive nodes", () => {
    const values = [
      "primitive string",
      1,
      -1,
      0,
      true,
      false,
      null,
      undefined,
      symbolStubs.get("Symbol"),
    ];

    const nodes = values.map((value, i) =>
      createNode({
        name: `root${i}`,
        contents: { value },
      })
    );

    nodes.forEach(node => expect(shouldLoadItemFullText(node)).toBeFalsy());
  });
});
