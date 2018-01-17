/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const expect = require("expect");

const actions = require("devtools/client/webconsole/new-console-output/actions/index");
const { messagesAdd } = require("devtools/client/webconsole/new-console-output/actions/index");
const { ConsoleCommand } = require("devtools/client/webconsole/new-console-output/types");
const { getVisibleMessages } = require("devtools/client/webconsole/new-console-output/selectors/messages");
const { getAllFilters } = require("devtools/client/webconsole/new-console-output/selectors/filters");
const { setupStore } = require("devtools/client/webconsole/new-console-output/test/helpers");
const { FILTERS, PREFS } = require("devtools/client/webconsole/new-console-output/constants");
const { stubPackets } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");
const { stubPreparedMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");
const ServicesMock = require("Services");

describe("Filtering", () => {
  let store;
  let numMessages;
  // Number of messages in prepareBaseStore which are not filtered out, i.e. Evaluation
  // Results, console commands and console.groups .
  const numUnfilterableMessages = 3;

  beforeEach(() => {
    store = prepareBaseStore();
    store.dispatch(actions.filtersClear());
    numMessages = getVisibleMessages(store.getState()).length;
  });

  /**
   * Tests for filter buttons in Console toolbar. The test switches off
   * all filters and consequently tests one by one on the list of messages
   * created in `prepareBaseStore` method.
   */
  describe("Level filter", () => {
    beforeEach(() => {
      // Switch off all filters (include those which are on by default).
      store.dispatch(actions.filtersClear());
      store.dispatch(actions.filterToggle(FILTERS.DEBUG));
      store.dispatch(actions.filterToggle(FILTERS.ERROR));
      store.dispatch(actions.filterToggle(FILTERS.INFO));
      store.dispatch(actions.filterToggle(FILTERS.LOG));
      store.dispatch(actions.filterToggle(FILTERS.WARN));

      let messages = getVisibleMessages(store.getState());
      expect(messages.length).toEqual(numUnfilterableMessages);
    });

    it("filters log messages", () => {
      store.dispatch(actions.filterToggle(FILTERS.LOG));

      let messages = getVisibleMessages(store.getState());
      expect(messages.length).toEqual(numUnfilterableMessages + 5);
    });

    it("filters debug messages", () => {
      store.dispatch(actions.filterToggle(FILTERS.DEBUG));

      let messages = getVisibleMessages(store.getState());
      expect(messages.length).toEqual(numUnfilterableMessages + 1);
    });

    it("filters info messages", () => {
      store.dispatch(actions.filterToggle(FILTERS.INFO));

      let messages = getVisibleMessages(store.getState());
      expect(messages.length).toEqual(numUnfilterableMessages + 1);
    });

    it("filters warning messages", () => {
      store.dispatch(actions.filterToggle(FILTERS.WARN));

      let messages = getVisibleMessages(store.getState());
      expect(messages.length).toEqual(numUnfilterableMessages + 1);
    });

    it("filters error messages", () => {
      store.dispatch(actions.filterToggle(FILTERS.ERROR));

      let messages = getVisibleMessages(store.getState());
      expect(messages.length).toEqual(numUnfilterableMessages + 3);
    });

    it("filters css messages", () => {
      let message = stubPreparedMessages.get(
        "Unknown property ‘such-unknown-property’.  Declaration dropped."
      );
      store.dispatch(messagesAdd([message]));

      let messages = getVisibleMessages(store.getState());
      expect(messages.length).toEqual(numUnfilterableMessages);

      store.dispatch(actions.filterToggle("css"));
      messages = getVisibleMessages(store.getState());
      expect(messages.length).toEqual(numUnfilterableMessages + 1);
    });

    it("filters xhr messages", () => {
      let message = stubPreparedMessages.get("XHR GET request");
      store.dispatch(messagesAdd([message]));

      let messages = getVisibleMessages(store.getState());
      expect(messages.length).toEqual(numUnfilterableMessages);

      store.dispatch(actions.filterToggle("netxhr"));
      messages = getVisibleMessages(store.getState());
      expect(messages.length).toEqual(numUnfilterableMessages + 1);
    });

    it("filters network messages", () => {
      let message = stubPreparedMessages.get("GET request");
      store.dispatch(messagesAdd([message]));

      let messages = getVisibleMessages(store.getState());
      expect(messages.length).toEqual(numUnfilterableMessages);

      store.dispatch(actions.filterToggle("net"));
      messages = getVisibleMessages(store.getState());
      expect(messages.length).toEqual(numUnfilterableMessages + 1);
    });
  });

  describe("Text filter", () => {
    it("set the expected property on the store", () => {
      store.dispatch(actions.filterTextSet("danger"));
      expect(getAllFilters(store.getState()).text).toEqual("danger");
    });

    it("matches on value grips", () => {
      store.dispatch(actions.filterTextSet("danger"));
      let messages = getVisibleMessages(store.getState());
      expect(messages.length - numUnfilterableMessages).toEqual(1);

      // Checks that trimming works.
      store.dispatch(actions.filterTextSet("    danger    "));
      messages = getVisibleMessages(store.getState());
      expect(messages.length - numUnfilterableMessages).toEqual(1);
    });

    it("matches unicode values", () => {
      store.dispatch(actions.filterTextSet("鼬"));

      let messages = getVisibleMessages(store.getState());
      expect(messages.length - numUnfilterableMessages).toEqual(1);
    });

    it("matches locations", () => {
      // Add a message with a different filename.
      let locationMsg =
        Object.assign({}, stubPackets.get("console.log('foobar', 'test')"));
      locationMsg.message =
        Object.assign({}, locationMsg.message, { filename: "search-location-test.js" });
      store.dispatch(messagesAdd([locationMsg]));

      store.dispatch(actions.filterTextSet("search-location-test.js"));

      let messages = getVisibleMessages(store.getState());
      expect(messages.length - numUnfilterableMessages).toEqual(1);
    });

    it("matches stacktrace functionName", () => {
      let traceMessage = stubPackets.get("console.trace()");
      store.dispatch(messagesAdd([traceMessage]));

      store.dispatch(actions.filterTextSet("testStacktraceFiltering"));

      let messages = getVisibleMessages(store.getState());
      expect(messages.length - numUnfilterableMessages).toEqual(1);
    });

    it("matches stacktrace location", () => {
      let traceMessage = stubPackets.get("console.trace()");
      traceMessage.message =
        Object.assign({}, traceMessage.message, {
          filename: "search-location-test.js",
          lineNumber: 85,
          columnNumber: 13
        });

      store.dispatch(messagesAdd([traceMessage]));

      store.dispatch(actions.filterTextSet("search-location-test.js:85:13"));

      let messages = getVisibleMessages(store.getState());
      expect(messages.length - numUnfilterableMessages).toEqual(1);
    });

    it("restores all messages once text is cleared", () => {
      store.dispatch(actions.filterTextSet("danger"));
      store.dispatch(actions.filterTextSet(""));

      let messages = getVisibleMessages(store.getState());
      expect(messages.length).toEqual(numMessages);
    });
  });

  describe("Combined filters", () => {
    // @TODO add test
    it("filters");
  });
});

describe("Clear filters", () => {
  it("clears all filters", () => {
    const store = setupStore();

    // Setup test case
    store.dispatch(actions.filterToggle(FILTERS.ERROR));
    store.dispatch(actions.filterToggle(FILTERS.CSS));
    store.dispatch(actions.filterToggle(FILTERS.NET));
    store.dispatch(actions.filterToggle(FILTERS.NETXHR));
    store.dispatch(actions.filterTextSet("foobar"));

    expect(getAllFilters(store.getState())).toEqual({
      // default
      [FILTERS.WARN]: true,
      [FILTERS.LOG]: true,
      [FILTERS.INFO]: true,
      [FILTERS.DEBUG]: true,
      // changed
      [FILTERS.ERROR]: false,
      [FILTERS.CSS]: true,
      [FILTERS.NET]: true,
      [FILTERS.NETXHR]: true,
      [FILTERS.TEXT]: "foobar",
    });
    expect(ServicesMock.prefs.testHelpers.getFiltersPrefs()).toEqual({
      [PREFS.FILTER.WARN]: true,
      [PREFS.FILTER.LOG]: true,
      [PREFS.FILTER.INFO]: true,
      [PREFS.FILTER.DEBUG]: true,
      [PREFS.FILTER.ERROR]: false,
      [PREFS.FILTER.CSS]: true,
      [PREFS.FILTER.NET]: true,
      [PREFS.FILTER.NETXHR]: true,
    });

    store.dispatch(actions.filtersClear());

    expect(getAllFilters(store.getState())).toEqual({
      [FILTERS.CSS]: false,
      [FILTERS.DEBUG]: true,
      [FILTERS.ERROR]: true,
      [FILTERS.INFO]: true,
      [FILTERS.LOG]: true,
      [FILTERS.NET]: false,
      [FILTERS.NETXHR]: false,
      [FILTERS.WARN]: true,
      [FILTERS.TEXT]: "",
    });

    expect(ServicesMock.prefs.testHelpers.getFiltersPrefs()).toEqual({
      [PREFS.FILTER.CSS]: false,
      [PREFS.FILTER.DEBUG]: true,
      [PREFS.FILTER.ERROR]: true,
      [PREFS.FILTER.INFO]: true,
      [PREFS.FILTER.LOG]: true,
      [PREFS.FILTER.NET]: false,
      [PREFS.FILTER.NETXHR]: false,
      [PREFS.FILTER.WARN]: true,
    });
  });
});

describe("Resets filters", () => {
  it("resets default filters value to their original one.", () => {
    const store = setupStore();

    // Setup test case
    store.dispatch(actions.filterToggle(FILTERS.ERROR));
    store.dispatch(actions.filterToggle(FILTERS.LOG));
    store.dispatch(actions.filterToggle(FILTERS.CSS));
    store.dispatch(actions.filterToggle(FILTERS.NET));
    store.dispatch(actions.filterToggle(FILTERS.NETXHR));
    store.dispatch(actions.filterTextSet("foobar"));

    expect(getAllFilters(store.getState())).toEqual({
      // default
      [FILTERS.WARN]: true,
      [FILTERS.INFO]: true,
      [FILTERS.DEBUG]: true,
      // changed
      [FILTERS.ERROR]: false,
      [FILTERS.LOG]: false,
      [FILTERS.CSS]: true,
      [FILTERS.NET]: true,
      [FILTERS.NETXHR]: true,
      [FILTERS.TEXT]: "foobar",
    });

    expect(ServicesMock.prefs.testHelpers.getFiltersPrefs()).toEqual({
      [PREFS.FILTER.WARN]: true,
      [PREFS.FILTER.INFO]: true,
      [PREFS.FILTER.DEBUG]: true,
      [PREFS.FILTER.ERROR]: false,
      [PREFS.FILTER.LOG]: false,
      [PREFS.FILTER.CSS]: true,
      [PREFS.FILTER.NET]: true,
      [PREFS.FILTER.NETXHR]: true,
    });

    store.dispatch(actions.defaultFiltersReset());

    expect(getAllFilters(store.getState())).toEqual({
      // default
      [FILTERS.ERROR]: true,
      [FILTERS.WARN]: true,
      [FILTERS.LOG]: true,
      [FILTERS.INFO]: true,
      [FILTERS.DEBUG]: true,
      [FILTERS.TEXT]: "",
      // non-default filters weren't changed
      [FILTERS.CSS]: true,
      [FILTERS.NET]: true,
      [FILTERS.NETXHR]: true,
    });

    expect(ServicesMock.prefs.testHelpers.getFiltersPrefs()).toEqual({
      [PREFS.FILTER.ERROR]: true,
      [PREFS.FILTER.WARN]: true,
      [PREFS.FILTER.LOG]: true,
      [PREFS.FILTER.INFO]: true,
      [PREFS.FILTER.DEBUG]: true,
      [PREFS.FILTER.CSS]: true,
      [PREFS.FILTER.NET]: true,
      [PREFS.FILTER.NETXHR]: true,
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
    "console.group('bar')",
    "console.debug('debug message');",
    "console.info('info message');",
    "console.error('error message');",
    "console.table(['red', 'green', 'blue']);",
    "console.assert(false, {message: 'foobar'})",
  ]);

  // Console Command - never filtered
  store.dispatch(messagesAdd([new ConsoleCommand({ messageText: `console.warn("x")` })]));

  return store;
}
