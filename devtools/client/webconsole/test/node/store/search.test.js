/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const expect = require("expect");

const actions = require("resource://devtools/client/webconsole/actions/index.js");
const {
  getVisibleMessages,
} = require("resource://devtools/client/webconsole/selectors/messages.js");
const {
  setupStore,
} = require("resource://devtools/client/webconsole/test/node/helpers.js");

describe("Searching in grips", () => {
  let store;

  beforeEach(() => {
    store = prepareBaseStore();
    store.dispatch(actions.filtersClear());
  });

  describe("Search in table & array & object props", () => {
    it("matches on value grips", () => {
      store.dispatch(actions.filterTextSet("red"));
      expect(getVisibleMessages(store.getState()).length).toEqual(3);
    });
  });

  describe("Search in object value", () => {
    it("matches on value grips", () => {
      store.dispatch(actions.filterTextSet("redValue"));
      expect(getVisibleMessages(store.getState()).length).toEqual(1);
    });
  });

  describe("Search in regex", () => {
    it("matches on value grips", () => {
      store.dispatch(actions.filterTextSet("a.b.c"));
      expect(getVisibleMessages(store.getState()).length).toEqual(1);
    });
  });

  describe("Search in map values", () => {
    it("matches on value grips", () => {
      store.dispatch(actions.filterTextSet("value1"));
      expect(getVisibleMessages(store.getState()).length).toEqual(1);
    });
  });

  describe("Search in map keys", () => {
    it("matches on value grips", () => {
      store.dispatch(actions.filterTextSet("key1"));
      expect(getVisibleMessages(store.getState()).length).toEqual(1);
    });
  });

  describe("Search in text", () => {
    it("matches on value grips", () => {
      store.dispatch(actions.filterTextSet("myobj"));
      expect(getVisibleMessages(store.getState()).length).toEqual(1);
    });
  });

  describe("Search in logs with net messages", () => {
    it("matches on network messages", () => {
      store.dispatch(actions.filterToggle("net"));
      store.dispatch(actions.filterTextSet("get"));
      expect(getVisibleMessages(store.getState()).length).toEqual(1);
    });
  });

  describe("Search in frame", () => {
    it("matches on file name", () => {
      store.dispatch(actions.filterTextSet("test-console-api.html:1:35"));
      expect(getVisibleMessages(store.getState()).length).toEqual(7);
    });

    it("do not match on full url", () => {
      store.dispatch(
        actions.filterTextSet("https://example.com/browser/devtools")
      );
      expect(getVisibleMessages(store.getState()).length).toEqual(0);
    });
  });

  describe("Reverse search", () => {
    it("reverse matches on value grips", () => {
      store.dispatch(actions.filterTextSet("-red"));
      expect(getVisibleMessages(store.getState()).length).toEqual(6);
    });

    it("reverse matches on file name", () => {
      store.dispatch(actions.filterTextSet("-test-console-api.html:1:35"));
      expect(getVisibleMessages(store.getState()).length).toEqual(2);
    });
  });
});

function prepareBaseStore() {
  const store = setupStore([
    "console.log('foobar', 'test')",
    "console.warn('danger, will robinson!')",
    "console.table(['red', 'green', 'blue']);",
    "console.count('bar')",
    "console.log('myarray', ['red', 'green', 'blue'])",
    "console.log('myregex', /a.b.c/)",
    "console.log('mymap')",
    "console.log('myobject', {red: 'redValue', green: 'greenValue', blue: 'blueValue'});",
    "GET request update",
  ]);

  return store;
}
