/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for targets management on the toolbox store.
 */

const { createToolboxStore } = require("devtools/client/framework/store");
const actions = require("devtools/client/framework/actions/targets");
const {
  getSelectedTarget,
  getToolboxTargets,
} = require("devtools/client/framework/reducers/targets");

describe("Toolbox store - targets", () => {
  describe("registerTarget", () => {
    it("adds the target to the list", () => {
      const store = createToolboxStore();

      const targetFront1 = {
        actorID: "target/1",
      };

      store.dispatch(actions.registerTarget(targetFront1));

      let targets = getToolboxTargets(store.getState());
      expect(targets.length).toEqual(1);
      expect(targets[0].actorID).toEqual("target/1");

      const targetFront2 = {
        actorID: "target/2",
      };

      store.dispatch(actions.registerTarget(targetFront2));

      targets = getToolboxTargets(store.getState());
      expect(targets.length).toEqual(2);
      expect(targets[0].actorID).toEqual("target/1");
      expect(targets[1].actorID).toEqual("target/2");
    });
  });

  describe("selectTarget", () => {
    it("updates the selected property when the target is known", () => {
      const store = createToolboxStore();
      const targetFront1 = {
        actorID: "target/1",
      };
      store.dispatch(actions.registerTarget(targetFront1));
      store.dispatch(actions.selectTarget("target/1"));
      expect(getSelectedTarget(store.getState()).actorID).toBe("target/1");
    });

    it("does not update the selected property when the target is unknown", () => {
      const store = createToolboxStore();
      const targetFront1 = {
        actorID: "target/1",
      };
      store.dispatch(actions.registerTarget(targetFront1));
      store.dispatch(actions.selectTarget("target/1"));
      expect(getSelectedTarget(store.getState()).actorID).toBe("target/1");

      store.dispatch(actions.selectTarget("target/unknown"));
      expect(getSelectedTarget(store.getState()).actorID).toBe("target/1");
    });

    it("does not update the state when the target is already selected", () => {
      const store = createToolboxStore();
      const targetFront1 = {
        actorID: "target/1",
      };
      store.dispatch(actions.registerTarget(targetFront1));
      store.dispatch(actions.selectTarget("target/1"));

      const state = store.getState();
      store.dispatch(actions.selectTarget("target/1"));
      expect(store.getState()).toStrictEqual(state);
    });
  });

  describe("unregisterTarget", () => {
    it("removes the target from the list", () => {
      const store = createToolboxStore();

      const targetFront1 = {
        actorID: "target/1",
      };
      const targetFront2 = {
        actorID: "target/2",
      };

      store.dispatch(actions.registerTarget(targetFront1));
      store.dispatch(actions.registerTarget(targetFront2));

      let targets = getToolboxTargets(store.getState());
      expect(targets.length).toEqual(2);

      store.dispatch(actions.unregisterTarget(targetFront1));
      targets = getToolboxTargets(store.getState());
      expect(targets.length).toEqual(1);
      expect(targets[0].actorID).toEqual("target/2");

      store.dispatch(actions.unregisterTarget(targetFront2));
      expect(getToolboxTargets(store.getState()).length).toEqual(0);
    });

    it("does not update the state when the target is unknown", () => {
      const store = createToolboxStore();

      const targetFront1 = {
        actorID: "target/1",
      };
      const targetFront2 = {
        actorID: "target/unknown",
      };

      store.dispatch(actions.registerTarget(targetFront1));

      const state = store.getState();
      store.dispatch(actions.unregisterTarget(targetFront2));
      expect(store.getState()).toStrictEqual(state);
    });

    it("resets the selected property when it was the selected target", () => {
      const store = createToolboxStore();

      const targetFront1 = {
        actorID: "target/1",
      };

      store.dispatch(actions.registerTarget(targetFront1));
      store.dispatch(actions.selectTarget("target/1"));
      expect(getSelectedTarget(store.getState()).actorID).toBe("target/1");

      store.dispatch(actions.unregisterTarget(targetFront1));
      expect(getSelectedTarget(store.getState())).toBe(null);
    });
  });
});
