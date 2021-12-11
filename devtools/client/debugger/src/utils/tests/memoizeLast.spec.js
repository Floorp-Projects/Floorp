/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { memoizeLast } from "../memoizeLast";

const a = { number: 3 };
const b = { number: 4 };

function add(...numberObjects) {
  return numberObjects.reduce((prev, cur) => prev + cur.number, 0);
}

describe("memozie", () => {
  it("should re-calculate when a value changes", () => {
    const mAdd = memoizeLast(add);
    mAdd(a);
    expect(mAdd(a)).toEqual(3);
    mAdd(b);
    expect(mAdd(b)).toEqual(4);
  });

  it("should only run once", () => {
    const mockAdd = jest.fn(add);
    const mAdd = memoizeLast(mockAdd);
    mAdd(a);
    mAdd(a);

    expect(mockAdd.mock.calls[0]).toEqual([{ number: 3 }]);
  });
});
