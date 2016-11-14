/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const expect = require("expect");

const actions = require("devtools/client/webconsole/new-console-output/actions/index");
const { messageAdd } = require("devtools/client/webconsole/new-console-output/actions/index");
const { ConsoleCommand } = require("devtools/client/webconsole/new-console-output/types");
const { getAllMessages } = require("devtools/client/webconsole/new-console-output/selectors/messages");
const { getAllFilters } = require("devtools/client/webconsole/new-console-output/selectors/filters");
const { setupStore } = require("devtools/client/webconsole/new-console-output/test/helpers");
const { MESSAGE_LEVEL } = require("devtools/client/webconsole/new-console-output/constants");
const { stubPackets } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");
const { stubPreparedMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");

describe("Filtering", () => {
  let store;
  let numMessages;
  // Number of messages in prepareBaseStore which are not filtered out, i.e. Evaluation
  // Results, console commands and console.groups .
  const numUnfilterableMessages = 3;

  beforeEach(() => {
    store = prepareBaseStore();
    store.dispatch(actions.filtersClear());
    numMessages = getAllMessages(store.getState()).size;
  });

  describe("Level filter", () => {
    it("filters log messages", () => {
      store.dispatch(actions.filterToggle(MESSAGE_LEVEL.LOG));

      let messages = getAllMessages(store.getState());
      expect(messages.size).toEqual(numMessages - 3);
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

    it("filters css messages", () => {
      let message = stubPreparedMessages.get(
        "Unknown property ‘such-unknown-property’.  Declaration dropped."
      );
      store.dispatch(messageAdd(message));

      let messages = getAllMessages(store.getState());
      expect(messages.size).toEqual(numMessages);

      store.dispatch(actions.filterToggle("css"));
      messages = getAllMessages(store.getState());
      expect(messages.size).toEqual(numMessages + 1);
    });

    it("filters xhr messages", () => {
      let message = stubPreparedMessages.get("XHR GET request");
      store.dispatch(messageAdd(message));

      let messages = getAllMessages(store.getState());
      expect(messages.size).toEqual(numMessages);

      store.dispatch(actions.filterToggle("netxhr"));
      messages = getAllMessages(store.getState());
      expect(messages.size).toEqual(numMessages + 1);
    });

    it("filters network messages", () => {
      let message = stubPreparedMessages.get("GET request");
      store.dispatch(messageAdd(message));

      let messages = getAllMessages(store.getState());
      expect(messages.size).toEqual(numMessages);

      store.dispatch(actions.filterToggle("net"));
      messages = getAllMessages(store.getState());
      expect(messages.size).toEqual(numMessages + 1);
    });
  });

  describe("Text filter", () => {
    it("matches on value grips", () => {
      store.dispatch(actions.filterTextSet("danger"));

      let messages = getAllMessages(store.getState());
      expect(messages.size - numUnfilterableMessages).toEqual(1);
    });

    it("matches unicode values", () => {
      store.dispatch(actions.filterTextSet("鼬"));

      let messages = getAllMessages(store.getState());
      expect(messages.size - numUnfilterableMessages).toEqual(1);
    });

    it("matches locations", () => {
      // Add a message with a different filename.
      let locationMsg =
        Object.assign({}, stubPackets.get("console.log('foobar', 'test')"));
      locationMsg.message =
        Object.assign({}, locationMsg.message, { filename: "search-location-test.js" });
      store.dispatch(messageAdd(locationMsg));

      store.dispatch(actions.filterTextSet("search-location-test.js"));

      let messages = getAllMessages(store.getState());
      expect(messages.size - numUnfilterableMessages).toEqual(1);
    });

    it("matches stacktrace functionName", () => {
      let traceMessage = stubPackets.get("console.trace()");
      store.dispatch(messageAdd(traceMessage));

      store.dispatch(actions.filterTextSet("testStacktraceFiltering"));

      let messages = getAllMessages(store.getState());
      expect(messages.size - numUnfilterableMessages).toEqual(1);
    });

    it("matches stacktrace location", () => {
      let traceMessage = stubPackets.get("console.trace()");
      traceMessage.message =
        Object.assign({}, traceMessage.message, {
          filename: "search-location-test.js",
          lineNumber: 85,
          columnNumber: 13
        });

      store.dispatch(messageAdd(traceMessage));

      store.dispatch(actions.filterTextSet("search-location-test.js:85:13"));

      let messages = getAllMessages(store.getState());
      expect(messages.size - numUnfilterableMessages).toEqual(1);
    });

    it("restores all messages once text is cleared", () => {
      store.dispatch(actions.filterTextSet("danger"));
      store.dispatch(actions.filterTextSet(""));

      let messages = getAllMessages(store.getState());
      expect(messages.size).toEqual(numMessages);
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
    store.dispatch(actions.filterToggle("netxhr"));
    store.dispatch(actions.filterTextSet("foobar"));

    let filters = getAllFilters(store.getState());
    expect(filters.toJS()).toEqual({
      "css": true,
      "debug": true,
      "error": false,
      "info": true,
      "log": true,
      "net": false,
      "netxhr": true,
      "warn": true,
      "text": "foobar"
    });

    store.dispatch(actions.filtersClear());

    filters = getAllFilters(store.getState());
    expect(filters.toJS()).toEqual({
      "css": false,
      "debug": true,
      "error": true,
      "info": true,
      "log": true,
      "net": false,
      "netxhr": false,
      "warn": true,
      "text": ""
    });
  });
});

function prepareBaseStore() {
  const store = setupStore([
    // Console API
    "console.log('foobar', 'test')",
    "console.warn('danger, will robinson!')",
    "console.log(undefined)",
    "console.count('bar')",
    "console.log('鼬')",
    // Evaluation Result - never filtered
    "new Date(0)",
    // PageError
    "ReferenceError: asdf is not defined",
    "console.group('bar')"
  ]);

  // Console Command - never filtered
  store.dispatch(messageAdd(new ConsoleCommand({ messageText: `console.warn("x")` })));

  return store;
}
