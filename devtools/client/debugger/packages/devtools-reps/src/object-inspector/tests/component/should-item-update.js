/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global jest */

const { mountObjectInspector } = require("../test-utils");
const ObjectClient = require("../__mocks__/object-client");
const LongStringClient = require("../__mocks__/long-string-client");

const repsPath = "../../../reps";
const longStringStubs = require(`${repsPath}/stubs/long-string`);
const gripStubs = require(`${repsPath}/stubs/grip`);

function mount(stub) {
  const root = {
    path: "root",
    contents: {
      value: stub,
    },
  };

  const { wrapper } = mountObjectInspector({
    client: {
      createObjectClient: grip => ObjectClient(grip),
      createLongStringClient: grip => LongStringClient(grip),
    },
    props: {
      roots: [root],
    },
  });

  return { wrapper, root };
}

describe("shouldItemUpdate", () => {
  it("for longStrings", () => {
    shouldItemUpdateCheck(longStringStubs.get("testUnloadedFullText"), true, 2);
  });

  it("for basic object", () => {
    shouldItemUpdateCheck(gripStubs.get("testBasic"), false, 1);
  });
});

function shouldItemUpdateCheck(
  stub,
  shouldItemUpdateResult,
  renderCallsLength
) {
  const { root, wrapper } = mount(stub);

  const shouldItemUpdateSpy = getShouldItemUpdateSpy(wrapper);
  const treeNodeRenderSpy = getTreeNodeRenderSpy(wrapper);

  updateObjectInspectorTree(wrapper);

  checkShouldItemUpdate(shouldItemUpdateSpy, root, shouldItemUpdateResult);
  expect(treeNodeRenderSpy.mock.calls).toHaveLength(renderCallsLength);
}

function checkShouldItemUpdate(spy, item, result) {
  expect(spy.mock.calls).toHaveLength(1);
  expect(spy.mock.calls[0][0]).toBe(item);
  expect(spy.mock.calls[0][1]).toBe(item);
  expect(spy.mock.results[0].value).toBe(result);
}

function getInstance(wrapper, selector) {
  return wrapper
    .find(selector)
    .first()
    .instance();
}

function getShouldItemUpdateSpy(wrapper) {
  return jest.spyOn(
    getInstance(wrapper, "ObjectInspector"),
    "shouldItemUpdate"
  );
}

function getTreeNodeRenderSpy(wrapper) {
  return jest.spyOn(getInstance(wrapper, "TreeNode"), "render");
}

function updateObjectInspectorTree(wrapper) {
  // Update the ObjectInspector first to propagate its updated options to the
  // Tree component.
  getInstance(wrapper, "ObjectInspector").forceUpdate();
  getInstance(wrapper, "Tree").forceUpdate();
}
