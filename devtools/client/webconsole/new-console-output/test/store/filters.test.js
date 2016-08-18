/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const expect = require("expect");

const actions = require("devtools/client/webconsole/new-console-output/actions/filters");
const { messageAdd } = require("devtools/client/webconsole/new-console-output/actions/messages");
const { ConsoleCommand } = require("devtools/client/webconsole/new-console-output/types");
const { getAllMessages } = require("devtools/client/webconsole/new-console-output/selectors/messages");
const { getAllFilters } = require("devtools/client/webconsole/new-console-output/selectors/filters");
const { setupStore } = require("devtools/client/webconsole/new-console-output/test/helpers");
const { MESSAGE_LEVEL } = require("devtools/client/webconsole/new-console-output/constants");

describe("Filtering", () => {
  const numMessages = 7;
  const store = setupStore([
    // Console API
    "console.log('foobar', 'test')",
    "console.warn('danger, will robinson!')",
    "console.log(undefined)",
    "console.count('bar')",
    // Evaluation Result
    "new Date(0)",
    // PageError
    "ReferenceError"
  ]);
  // Console Command
  store.dispatch(messageAdd(new ConsoleCommand({ messageText: `console.warn("x")` })));

  beforeEach(() => {
    store.dispatch(actions.filtersClear());
  });

  describe("Level filter", () => {
    it("filters log messages", () => {
      store.dispatch(actions.filterToggle(MESSAGE_LEVEL.LOG));

      let messages = getAllMessages(store.getState());
      expect(messages.size).toEqual(numMessages - 2);
    });

    it("filters debug messages", () => {
      store.dispatch(actions.filterToggle(MESSAGE_LEVEL.DEBUG));

      let messages = getAllMessages(store.getState());
      expect(messages.size).toEqual(numMessages - 1);
    });

    // @TODO add info stub
    it("filters info messages");

    it("filters warning messages", () => {
      store.dispatch(actions.filterToggle(MESSAGE_LEVEL.WARN));

      let messages = getAllMessages(store.getState());
      expect(messages.size).toEqual(numMessages - 1);
    });

    it("filters error messages", () => {
      store.dispatch(actions.filterToggle(MESSAGE_LEVEL.ERROR));

      let messages = getAllMessages(store.getState());
      expect(messages.size).toEqual(numMessages - 1);
    });
  });

  describe("Text filter", () => {
    it("matches on value grips", () => {
      store.dispatch(actions.filterTextSet("danger"));

      let messages = getAllMessages(store.getState());
      // @TODO figure out what this should filter
      // This does not filter out PageErrors, Evaluation Results or console commands
      expect(messages.size).toEqual(5);
    });
  });

  describe("Combined filters", () => {
    // @TODO add test
    it("filters");
  });
});

describe("Clear filters", () => {
  it("clears all filters", () => {
    const store = setupStore([]);

    // Setup test case
    store.dispatch(actions.filterToggle(MESSAGE_LEVEL.ERROR));
    store.dispatch(actions.filterTextSet("foobar"));
    let filters = getAllFilters(store.getState());
    expect(filters.toJS()).toEqual({
      "debug": true,
      "error": false,
      "info": true,
      "log": true,
      "warn": true,
      "text": "foobar"
    });

    store.dispatch(actions.filtersClear());

    filters = getAllFilters(store.getState());
    expect(filters.toJS()).toEqual({
      "debug": true,
      "error": true,
      "info": true,
      "log": true,
      "warn": true,
      "text": ""
    });
  });
});
