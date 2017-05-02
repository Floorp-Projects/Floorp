/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const expect = require("expect");

const actions = require("devtools/client/webconsole/new-console-output/actions/index");
const { getAllMessages } = require("devtools/client/webconsole/new-console-output/selectors/messages");
const { setupStore } = require("devtools/client/webconsole/new-console-output/test/helpers");

describe("Searching in grips", () => {
  let store;

  beforeEach(() => {
    store = prepareBaseStore();
    store.dispatch(actions.filtersClear());
  });

  describe("Search in table & array & object props", () => {
    it("matches on value grips", () => {
      store.dispatch(actions.filterTextSet("red"));
      expect(getAllMessages(store.getState()).size).toEqual(3);
    });
  });

  describe("Search in object value", () => {
    it("matches on value grips", () => {
      store.dispatch(actions.filterTextSet("redValue"));
      expect(getAllMessages(store.getState()).size).toEqual(1);
    });
  });

  describe("Search in regex", () => {
    it("matches on value grips", () => {
      store.dispatch(actions.filterTextSet("a.b.c"));
      expect(getAllMessages(store.getState()).size).toEqual(1);
    });
  });

  describe("Search in map values", () => {
    it("matches on value grips", () => {
      store.dispatch(actions.filterTextSet("value1"));
      expect(getAllMessages(store.getState()).size).toEqual(1);
    });
  });

  describe("Search in map keys", () => {
    it("matches on value grips", () => {
      store.dispatch(actions.filterTextSet("key1"));
      expect(getAllMessages(store.getState()).size).toEqual(1);
    });
  });

  describe("Search in text", () => {
    it("matches on value grips", () => {
      store.dispatch(actions.filterTextSet("myobj"));
      expect(getAllMessages(store.getState()).size).toEqual(1);
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
    "console.map('mymap')",
    "console.log('myobject', {red: 'redValue', green: 'greenValue', blue: 'blueValue'});",
  ]);

  return store;
}
