/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const expect = require("expect");

const actions = require("devtools/client/webconsole/new-console-output/actions/index");
const { setupStore } = require("devtools/client/webconsole/new-console-output/test/helpers");

describe("Testing UI", () => {
  let store;

  beforeEach(() => {
    store = setupStore();
  });

  describe("Toggle sidebar", () => {
    it("sidebar is toggled on and off", () => {
      store.dispatch(actions.sidebarToggle());
      expect(store.getState().ui.sidebarVisible).toEqual(true);
      store.dispatch(actions.sidebarToggle());
      expect(store.getState().ui.sidebarVisible).toEqual(false);
    });
  });

  describe("Hide sidebar on clear", () => {
    it("sidebar is hidden on clear", () => {
      store.dispatch(actions.sidebarToggle());
      expect(store.getState().ui.sidebarVisible).toEqual(true);
      store.dispatch(actions.messagesClear());
      expect(store.getState().ui.sidebarVisible).toEqual(false);
      store.dispatch(actions.messagesClear());
      expect(store.getState().ui.sidebarVisible).toEqual(false);
    });
  });
});
