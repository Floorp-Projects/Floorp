/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for threads management on the toolbox store.
 */

const { createToolboxStore } = require("devtools/client/framework/store");
const actions = require("devtools/client/framework/actions/threads");
const {
  getSelectedThread,
  getToolboxThreads,
} = require("devtools/client/framework/reducers/threads");

describe("Toolbox store - threads", () => {
  describe("registerThread", () => {
    it("adds the thread to the list", async () => {
      const store = createToolboxStore();

      const targetFront1 = getTargetFrontMock({
        actorID: "thread/1",
      });

      await store.dispatch(actions.registerThread(targetFront1));

      let threads = getToolboxThreads(store.getState());
      expect(threads.length).toEqual(1);
      expect(threads[0].actorID).toEqual("thread/1");

      const targetFront2 = getTargetFrontMock({
        actorID: "thread/2",
      });

      await store.dispatch(actions.registerThread(targetFront2));

      threads = getToolboxThreads(store.getState());
      expect(threads.length).toEqual(2);
      expect(threads[0].actorID).toEqual("thread/1");
      expect(threads[1].actorID).toEqual("thread/2");
    });
  });

  describe("selectThread", () => {
    it("updates the selected property when the thread is known", async () => {
      const store = createToolboxStore();
      const targetFront1 = getTargetFrontMock({
        actorID: "thread/1",
      });
      await store.dispatch(actions.registerThread(targetFront1));
      store.dispatch(actions.selectThread("thread/1"));
      expect(getSelectedThread(store.getState()).actorID).toBe("thread/1");
    });

    it("does not update the selected property when the thread is unknown", async () => {
      const store = createToolboxStore();
      const targetFront1 = getTargetFrontMock({
        actorID: "thread/1",
      });
      await store.dispatch(actions.registerThread(targetFront1));
      store.dispatch(actions.selectThread("thread/1"));
      expect(getSelectedThread(store.getState()).actorID).toBe("thread/1");

      store.dispatch(actions.selectThread("thread/unknown"));
      expect(getSelectedThread(store.getState()).actorID).toBe("thread/1");
    });

    it("does not update the state when the thread is already selected", async () => {
      const store = createToolboxStore();
      const targetFront1 = getTargetFrontMock({
        actorID: "thread/1",
      });
      await store.dispatch(actions.registerThread(targetFront1));
      store.dispatch(actions.selectThread("thread/1"));

      const state = store.getState();
      store.dispatch(actions.selectThread("thread/1"));
      expect(store.getState()).toStrictEqual(state);
    });
  });

  describe("clearThread", () => {
    it("removes the thread from the list", async () => {
      const store = createToolboxStore();

      const targetFront1 = getTargetFrontMock({
        actorID: "thread/1",
      });
      const targetFront2 = getTargetFrontMock({
        actorID: "thread/2",
      });

      await store.dispatch(actions.registerThread(targetFront1));
      await store.dispatch(actions.registerThread(targetFront2));

      let threads = getToolboxThreads(store.getState());
      expect(threads.length).toEqual(2);

      store.dispatch(actions.clearThread(targetFront1));
      threads = getToolboxThreads(store.getState());
      expect(threads.length).toEqual(1);
      expect(threads[0].actorID).toEqual("thread/2");

      store.dispatch(actions.clearThread(targetFront2));
      expect(getToolboxThreads(store.getState()).length).toEqual(0);
    });

    it("does not update the state when the thread is unknown", async () => {
      const store = createToolboxStore();

      const targetFront1 = getTargetFrontMock({
        actorID: "thread/1",
      });
      const targetFront2 = getTargetFrontMock({
        actorID: "thread/unknown",
      });

      await store.dispatch(actions.registerThread(targetFront1));

      const state = store.getState();
      store.dispatch(actions.clearThread(targetFront2));
      expect(store.getState()).toStrictEqual(state);
    });

    it("resets the selected property when it was the selected thread", async () => {
      const store = createToolboxStore();

      const targetFront1 = getTargetFrontMock({
        actorID: "thread/1",
      });

      await store.dispatch(actions.registerThread(targetFront1));
      store.dispatch(actions.selectThread("thread/1"));
      expect(getSelectedThread(store.getState()).actorID).toBe("thread/1");

      store.dispatch(actions.clearThread(targetFront1));
      expect(getSelectedThread(store.getState())).toBe(null);
    });
  });
});

function getTargetFrontMock(threadData) {
  return {
    getFront: async function(typeName) {
      if (typeName === "thread") {
        return threadData;
      }
      return null;
    },
  };
}
