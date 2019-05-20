/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const gripArrayStubs = require("../../../reps/stubs/grip-array");
const gripMapStubs = require("../../../reps/stubs/grip-map");

const { createNode, nodeHasAllEntriesInPreview } = require("../../utils/node");

const createRootNode = value =>
  createNode({ name: "root", contents: { value } });
describe("nodeHasEntries", () => {
  it("returns true for a Map with every entries in the preview", () => {
    expect(
      nodeHasAllEntriesInPreview(
        createRootNode(gripMapStubs.get("testSymbolKeyedMap"))
      )
    ).toBe(true);
  });
  it("returns true for a WeakMap with every entries in the preview", () => {
    expect(
      nodeHasAllEntriesInPreview(
        createRootNode(gripMapStubs.get("testWeakMap"))
      )
    ).toBe(true);
  });
  it("returns true for a Set with every entries in the preview", () => {
    expect(
      nodeHasAllEntriesInPreview(
        createRootNode(gripArrayStubs.get("new Set([1,2,3,4])"))
      )
    ).toBe(true);
  });

  it("returns false for a Map with more than 10 items", () => {
    expect(
      nodeHasAllEntriesInPreview(
        createRootNode(gripMapStubs.get("testMoreThanMaxEntries"))
      )
    ).toBe(false);
  });

  it("returns false for a WeakSet with more than 10 items", () => {
    expect(
      nodeHasAllEntriesInPreview(
        createRootNode(
          gripArrayStubs.get(
            "new WeakSet(document.querySelectorAll('div, button'))"
          )
        )
      )
    ).toBe(false);
  });

  it("returns false for a null value", () => {
    expect(nodeHasAllEntriesInPreview(createRootNode(null))).toBe(false);
  });

  it("returns false for an undefined value", () => {
    expect(nodeHasAllEntriesInPreview(createRootNode(null))).toBe(false);
  });

  it("returns false for an empty object", () => {
    expect(nodeHasAllEntriesInPreview(createRootNode({}))).toBe(false);
  });

  it("returns false for an object with a preview without items", () => {
    expect(
      nodeHasAllEntriesInPreview(
        createRootNode({
          preview: {
            size: 1,
          },
        })
      )
    ).toBe(false);
  });
});
