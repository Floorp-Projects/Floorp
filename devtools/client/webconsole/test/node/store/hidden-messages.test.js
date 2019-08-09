/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const expect = require("expect");

const actions = require("devtools/client/webconsole/actions/index");
const {
  getFilteredMessagesCount,
} = require("devtools/client/webconsole/selectors/messages");
const { setupStore } = require("devtools/client/webconsole/test/node/helpers");
const { FILTERS } = require("devtools/client/webconsole/constants");
const {
  stubPackets,
} = require("devtools/client/webconsole/test/node/fixtures/stubs/index");

describe("Filtering - Hidden messages", () => {
  let store;

  beforeEach(() => {
    store = prepareBaseStore();
    // Switch off all filters (include those which are on by default).
    store.dispatch(actions.filtersClear());
    store.dispatch(actions.filterToggle(FILTERS.DEBUG));
    store.dispatch(actions.filterToggle(FILTERS.ERROR));
    store.dispatch(actions.filterToggle(FILTERS.INFO));
    store.dispatch(actions.filterToggle(FILTERS.LOG));
    store.dispatch(actions.filterToggle(FILTERS.WARN));
  });

  it("has the expected numbers", () => {
    const counter = getFilteredMessagesCount(store.getState());
    expect(counter).toEqual(BASIC_TEST_CASE_FILTERED_MESSAGE_COUNT);
  });

  it("has the expected numbers when there is a text search", () => {
    // "info" is disabled and the filter input only matches a warning message.
    store.dispatch(actions.filtersClear());
    store.dispatch(actions.filterToggle(FILTERS.INFO));
    store.dispatch(actions.filterTextSet("danger, will robinson!"));

    let counter = getFilteredMessagesCount(store.getState());
    expect(counter).toEqual({
      [FILTERS.ERROR]: 0,
      [FILTERS.WARN]: 0,
      [FILTERS.LOG]: 0,
      [FILTERS.INFO]: 1,
      [FILTERS.DEBUG]: 0,
      [FILTERS.TEXT]: 9,
      global: 10,
    });

    // Numbers update if the text search is cleared.
    store.dispatch(actions.filterTextSet(""));
    counter = getFilteredMessagesCount(store.getState());
    expect(counter).toEqual({
      [FILTERS.ERROR]: 0,
      [FILTERS.WARN]: 0,
      [FILTERS.LOG]: 0,
      [FILTERS.INFO]: 1,
      [FILTERS.DEBUG]: 0,
      [FILTERS.TEXT]: 0,
      global: 1,
    });
  });

  it("has the expected numbers when there's a text search on disabled categories", () => {
    store.dispatch(actions.filterTextSet("danger, will robinson!"));
    let counter = getFilteredMessagesCount(store.getState());
    expect(counter).toEqual({
      [FILTERS.ERROR]: 3,
      [FILTERS.WARN]: 1,
      [FILTERS.LOG]: 5,
      [FILTERS.INFO]: 1,
      [FILTERS.DEBUG]: 1,
      [FILTERS.TEXT]: 0,
      global: 11,
    });

    // Numbers update if the text search is cleared.
    store.dispatch(actions.filterTextSet(""));
    counter = getFilteredMessagesCount(store.getState());
    expect(counter).toEqual(BASIC_TEST_CASE_FILTERED_MESSAGE_COUNT);
  });

  it("updates when messages are added", () => {
    const packets = MESSAGES.map(key => stubPackets.get(key));
    store.dispatch(actions.messagesAdd(packets));

    const counter = getFilteredMessagesCount(store.getState());
    expect(counter).toEqual({
      [FILTERS.ERROR]: 6,
      [FILTERS.WARN]: 2,
      [FILTERS.LOG]: 10,
      [FILTERS.INFO]: 2,
      [FILTERS.DEBUG]: 2,
      [FILTERS.TEXT]: 0,
      global: 22,
    });
  });

  it("updates when filters are toggled", () => {
    store.dispatch(actions.filterToggle(FILTERS.LOG));

    let counter = getFilteredMessagesCount(store.getState());
    expect(counter).toEqual(
      Object.assign({}, BASIC_TEST_CASE_FILTERED_MESSAGE_COUNT, {
        [FILTERS.LOG]: 0,
        global: 6,
      })
    );

    store.dispatch(actions.filterToggle(FILTERS.ERROR));

    counter = getFilteredMessagesCount(store.getState());
    expect(counter).toEqual(
      Object.assign({}, BASIC_TEST_CASE_FILTERED_MESSAGE_COUNT, {
        [FILTERS.ERROR]: 0,
        [FILTERS.LOG]: 0,
        global: 3,
      })
    );

    store.dispatch(actions.filterToggle(FILTERS.LOG));
    store.dispatch(actions.filterToggle(FILTERS.ERROR));
    counter = getFilteredMessagesCount(store.getState());
    expect(counter).toEqual(BASIC_TEST_CASE_FILTERED_MESSAGE_COUNT);
  });

  it("has the expected numbers after message clear", () => {
    // Add a text search to make sure it is handled as well.
    store.dispatch(actions.filterTextSet("danger, will robinson!"));
    store.dispatch(actions.messagesClear());
    const counter = getFilteredMessagesCount(store.getState());
    expect(counter).toEqual({
      [FILTERS.ERROR]: 0,
      [FILTERS.WARN]: 0,
      [FILTERS.LOG]: 0,
      [FILTERS.INFO]: 0,
      [FILTERS.DEBUG]: 0,
      [FILTERS.TEXT]: 0,
      global: 0,
    });
  });

  it("has the expected numbers after filter clear", () => {
    // Add a text search to make sure it is handled as well.
    store.dispatch(actions.filterTextSet("danger, will robinson!"));
    store.dispatch(actions.filtersClear());
    const counter = getFilteredMessagesCount(store.getState());
    expect(counter).toEqual({
      [FILTERS.ERROR]: 0,
      [FILTERS.WARN]: 0,
      [FILTERS.LOG]: 0,
      [FILTERS.INFO]: 0,
      [FILTERS.DEBUG]: 0,
      [FILTERS.TEXT]: 0,
      global: 0,
    });
  });
});

const MESSAGES = [
  // Error
  "ReferenceError: asdf is not defined",
  "console.error('error message');",
  "console.assert(false, {message: 'foobar'})",
  // Warning
  "console.warn('danger, will robinson!')",
  // Log
  "console.log('foobar', 'test')",
  "console.log(undefined)",
  "console.count('bar')",
  "console.log('鼬')",
  "console.table(['red', 'green', 'blue']);",
  // Info
  "console.info('info message');",
  // Debug
  "console.debug('debug message');",
];

const BASIC_TEST_CASE_FILTERED_MESSAGE_COUNT = {
  [FILTERS.ERROR]: 3,
  [FILTERS.WARN]: 1,
  [FILTERS.LOG]: 5,
  [FILTERS.INFO]: 1,
  [FILTERS.DEBUG]: 1,
  [FILTERS.TEXT]: 0,
  global: 11,
};

function prepareBaseStore() {
  return setupStore(MESSAGES);
}
