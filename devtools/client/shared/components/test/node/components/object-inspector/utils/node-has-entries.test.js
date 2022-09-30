/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const gripArrayStubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/grip-array.js");
const gripMapStubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/grip-map.js");

const {
  createNode,
  nodeHasEntries,
} = require("resource://devtools/client/shared/components/object-inspector/utils/node.js");

const createRootNode = value =>
  createNode({ name: "root", contents: { value } });
describe("nodeHasEntries", () => {
  it("returns true for Maps", () => {
    expect(
      nodeHasEntries(createRootNode(gripMapStubs.get("testSymbolKeyedMap")))
    ).toBe(true);
  });

  it("returns true for WeakMaps", () => {
    expect(
      nodeHasEntries(createRootNode(gripMapStubs.get("testWeakMap")))
    ).toBe(true);
  });

  it("returns true for Sets", () => {
    expect(
      nodeHasEntries(createRootNode(gripArrayStubs.get("new Set([1,2,3,4])")))
    ).toBe(true);
  });

  it("returns true for WeakSets", () => {
    expect(
      nodeHasEntries(
        createRootNode(
          gripArrayStubs.get(
            "new WeakSet(document.querySelectorAll('div, button'))"
          )
        )
      )
    ).toBe(true);
  });

  it("returns false for Arrays", () => {
    expect(
      nodeHasEntries(createRootNode(gripMapStubs.get("testMaxProps")))
    ).toBe(false);
  });
});
