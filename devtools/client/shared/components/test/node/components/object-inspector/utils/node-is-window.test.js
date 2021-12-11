/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const gripWindowStubs = require("devtools/client/shared/components/test/node/stubs/reps/window");

const {
  createNode,
  nodeIsWindow,
} = require("devtools/client/shared/components/object-inspector/utils/node");

const createRootNode = value =>
  createNode({ name: "root", contents: { value } });
describe("nodeIsWindow", () => {
  it("returns true for Window", () => {
    expect(nodeIsWindow(createRootNode(gripWindowStubs.get("Window")))).toBe(
      true
    );
  });
});
