/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import fromJS from "../fromJS";

const preview = {
  kind: "ArrayLike",
  length: 201,
  items: [
    {
      type: "null",
    },
    "a test",
    "a",
    {
      type: "null",
    },
    {
      type: "null",
    },
    {
      type: "null",
    },
    {
      type: "null",
    },
    {
      type: "null",
    },
    {
      type: "null",
    },
    {
      type: "null",
    },
  ],
};

describe("fromJS", () => {
  it("supports array like objects", () => {
    const iPreview = fromJS(preview);
    expect(iPreview.get("length")).toEqual(201);
    expect(iPreview.get("items").size).toEqual(10);
  });

  it("supports arrays", () => {
    const iItems = fromJS(preview.items);
    expect(iItems.getIn([0, "type"])).toEqual("null");
    expect(iItems.size).toEqual(10);
  });

  it("supports objects without a prototype", () => {
    expect(() => fromJS(Object.create(null))).not.toThrow();
  });

  it("supports objects with `hasOwnProperty` fields", () => {
    const value = {
      lookupIterator: {
        value: {},
        writable: true,
      },

      hasOwnProperty: {
        value: {},
        writable: true,
      },
      arguments: {
        value: {},
        writable: false,
      },
    };

    const newMap = fromJS(value);
    expect(newMap.getIn(["hasOwnProperty", "writable"])).toEqual(true);
  });
});
