/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Sort, sortReducer} = require("../../src/reducers/sort");
const { SORT_BY } = require("../../src/constants");

describe("sorting reducer", () => {
  it("it should sort by sort type", () => {
    const initialState = new Sort();
    const action = {
      type: SORT_BY,
      sortType: "TimeWhen",
    };
    const expectedState = {
      type: "TimeWhen",
      ascending: true,
    };

    expect(expectedState).toEqual(sortReducer(initialState, action));
  });
});
