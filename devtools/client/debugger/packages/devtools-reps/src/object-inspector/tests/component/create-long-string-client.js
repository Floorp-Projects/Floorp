/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global jest */

const { mountObjectInspector } = require("../test-utils");
const ObjectClient = require("../__mocks__/object-client");
const LongStringClient = require("../__mocks__/long-string-client");

const repsPath = "../../../reps";
const longStringStubs = require(`${repsPath}/stubs/long-string`);

function mount(props) {
  const substring = jest.fn(() => Promise.resolve({ fullText: "" }));

  const client = {
    createObjectClient: grip => ObjectClient(grip),
    createLongStringClient: jest.fn(grip =>
      LongStringClient(grip, { substring })
    ),
  };

  const obj = mountObjectInspector({
    client,
    props,
  });

  return { ...obj, substring };
}

describe("createLongStringClient", () => {
  it("is called with the expected object for longString node", () => {
    const stub = longStringStubs.get("testUnloadedFullText");

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

    expect(client.createLongStringClient.mock.calls[0][0]).toBe(stub);
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

      // Third argument is the callback which holds the string response.
      expect(substring.mock.calls[0]).toHaveLength(3);
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
