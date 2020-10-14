/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { getValue } = require("../../utils/node");

describe("getValue", () => {
  it("get the value from contents.value", () => {
    let item = {
      contents: {
        value: "my value",
      },
    };
    expect(getValue(item)).toBe("my value");

    item = {
      contents: {
        value: 0,
      },
    };
    expect(getValue(item)).toBe(0);

    item = {
      contents: {
        value: false,
      },
    };
    expect(getValue(item)).toBe(false);

    item = {
      contents: {
        value: null,
      },
    };
    expect(getValue(item)).toBe(null);
  });

  it("get the value from contents.getterValue", () => {
    let item = {
      contents: {
        getterValue: "my getter value",
      },
    };
    expect(getValue(item)).toBe("my getter value");

    item = {
      contents: {
        getterValue: 0,
      },
    };
    expect(getValue(item)).toBe(0);

    item = {
      contents: {
        getterValue: false,
      },
    };
    expect(getValue(item)).toBe(false);

    item = {
      contents: {
        getterValue: null,
      },
    };
    expect(getValue(item)).toBe(null);
  });

  it("get the value from getter and setter", () => {
    let item = {
      contents: {
        get: "get",
      },
    };
    expect(getValue(item)).toEqual({ get: "get" });

    item = {
      contents: {
        set: "set",
      },
    };
    expect(getValue(item)).toEqual({ set: "set" });

    item = {
      contents: {
        get: "get",
        set: "set",
      },
    };
    expect(getValue(item)).toEqual({ get: "get", set: "set" });
  });
});
