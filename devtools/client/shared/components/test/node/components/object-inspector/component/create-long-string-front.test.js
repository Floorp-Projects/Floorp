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

function mount(props) {
  const substring = jest.fn(() => Promise.resolve(""));

  const client = {
    createObjectFront: grip => ObjectFront(grip),
    createLongStringFront: jest.fn(grip =>
      LongStringFront(grip, { substring })
    ),
  };

  const obj = mountObjectInspector({
    client,
    props,
  });

  return { ...obj, substring };
}

describe("createLongStringFront", () => {
  it("is called with the expected object for longString node", () => {
    const stub = longStringStubs.get("testMultiline");

    const { client } = mount({
      autoExpandDepth: 1,
      roots: [
        {
          path: "root",
          contents: {
            value: stub,
          },
        },
      ],
    });

    expect(client.createLongStringFront.mock.calls[0][0]).toBe(stub);
  });

  describe("substring", () => {
    it("is called for longStrings with unloaded full text", () => {
      const stub = longStringStubs.get("testUnloadedFullText");

      const { substring } = mount({
        autoExpandDepth: 1,
        roots: [
          {
            path: "root",
            contents: {
              value: stub,
            },
          },
        ],
      });

      expect(substring.mock.calls[0]).toHaveLength(2);
      const [start, length] = substring.mock.calls[0];
      expect(start).toBe(stub.initial.length);
      expect(length).toBe(stub.length);
    });

    it("is not called for longString node w/ loaded full text", () => {
      const stub = longStringStubs.get("testLoadedFullText");

      const { substring } = mount({
        autoExpandDepth: 1,
        roots: [
          {
            path: "root",
            contents: {
              value: stub,
            },
          },
        ],
      });

      expect(substring.mock.calls).toHaveLength(0);
    });
  });
});
