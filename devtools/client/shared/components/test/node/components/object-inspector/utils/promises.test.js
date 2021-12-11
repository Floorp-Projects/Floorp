/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const {
  makeNodesForPromiseProperties,
  nodeIsPromise,
} = require("devtools/client/shared/components/object-inspector/utils/node");

describe("promises utils function", () => {
  it("is promise", () => {
    const promise = {
      contents: {
        enumerable: true,
        configurable: false,
        value: {
          actor: "server2.conn2.child1/obj36",
          promiseState: {
            state: "rejected",
            reason: {
              type: "undefined",
            },
          },
          class: "Promise",
          type: "object",
        },
      },
    };

    expect(nodeIsPromise(promise)).toEqual(true);
  });

  it("makeNodesForPromiseProperties", () => {
    const item = {
      path: "root",
      contents: {
        value: {
          actor: "server2.conn2.child1/obj36",
          class: "Promise",
          type: "object",
        },
      },
    };
    const promiseState = {
      state: "rejected",
      reason: {
        type: "3",
      },
    };

    const properties = makeNodesForPromiseProperties({promiseState}, item);
    expect(properties).toMatchSnapshot();
  });
});
