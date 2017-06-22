/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const expect = require("expect");

const {
  debounceActions,
} = require("devtools/client/shared/redux/middleware/debounce");

describe("Debounce Middleware", () => {
  let nextArgs = [];
  const fakeStore = {};
  const fakeNext = (...args) => {
    nextArgs.push(args);
  };

  beforeEach(() => {
    nextArgs = [];
  });

  it("should pass the intercepted action to next", () => {
    const fakeAction = {
      type: "FAKE_ACTION"
    };

    debounceActions()(fakeStore)(fakeNext)(fakeAction);

    expect(nextArgs.length).toEqual(1);
    expect(nextArgs[0]).toEqual([fakeAction]);
  });

  it("should debounce if specified", () => {
    const fakeAction = {
      type: "FAKE_ACTION",
      meta: {
        debounce: true
      }
    };

    const executed = debounceActions(1, 1)(fakeStore)(fakeNext)(fakeAction);
    expect(nextArgs.length).toEqual(0);

    return executed.then(() => {
      expect(nextArgs.length).toEqual(1);
    });
  });

  it("should have no effect if no timeout", () => {
    const fakeAction = {
      type: "FAKE_ACTION",
      meta: {
        debounce: true
      }
    };

    debounceActions()(fakeStore)(fakeNext)(fakeAction);
    expect(nextArgs.length).toEqual(1);
    expect(nextArgs[0]).toEqual([fakeAction]);
  });
});
