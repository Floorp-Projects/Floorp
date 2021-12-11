/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global jest */

const {
  mountObjectInspector,
} = require("devtools/client/shared/components/test/node/components/object-inspector/test-utils");
const ObjectFront = require("devtools/client/shared/components/test/node/__mocks__/object-front");
const {
  LongStringFront,
} = require("devtools/client/shared/components/test/node/__mocks__/string-front");

const longStringStubs = require(`devtools/client/shared/components/test/node/stubs/reps/long-string`);
const gripStubs = require(`devtools/client/shared/components/test/node/stubs/reps/grip`);

function mount(stub) {
  const root = {
    path: "root",
    contents: {
      value: stub,
    },
  };

  const { wrapper } = mountObjectInspector({
    client: {
      createObjectFront: grip => ObjectFront(grip),
      createLongStringFront: grip => LongStringFront(grip),
      getFrontByID: _id => null,
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
