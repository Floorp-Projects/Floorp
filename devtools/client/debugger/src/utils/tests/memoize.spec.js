/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import memoize from "../memoize";

const a = { number: 3 };
const b = { number: 4 };
const c = { number: 5 };
const d = { number: 6 };

function add(...numberObjects) {
  return numberObjects.reduce((prev, cur) => prev + cur.number, 0);
}

describe("memozie", () => {
  it("should work for one arg as key", () => {
    const mAdd = memoize(add);
    mAdd(a);
    expect(mAdd(a)).toEqual(3);
    mAdd(b);
    expect(mAdd(b)).toEqual(4);
  });

  it("should only be called once", () => {
    const spy = jest.fn(() => 2);
    const mAdd = memoize(spy);

    mAdd(a);
    mAdd(a);
    mAdd(a);
    expect(spy).toHaveBeenCalledTimes(1);
  });

  it("should work for two args as key", () => {
    const mAdd = memoize(add);
    expect(mAdd(a, b)).toEqual(7);
    expect(mAdd(a, b)).toEqual(7);
    expect(mAdd(a, c)).toEqual(8);
  });

  it("should work with many args as key", () => {
    const mAdd = memoize(add);
    expect(mAdd(a, b, c)).toEqual(12);
    expect(mAdd(a, b, d)).toEqual(13);
    expect(mAdd(a, b, c)).toEqual(12);
  });
});
