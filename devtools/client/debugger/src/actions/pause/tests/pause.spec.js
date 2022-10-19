/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  actions,
  selectors,
  createStore,
  waitForState,
  makeSource,
  makeOriginalSource,
  makeFrame,
} from "../../../utils/test-head";

import { makeWhyNormal } from "../../../utils/test-mockup";

const { isStepping } = selectors;

let stepInResolve = null;
const mockCommandClient = {
  stepIn: () =>
    new Promise(_resolve => {
      stepInResolve = _resolve;
    }),
  stepOver: () => new Promise(_resolve => _resolve),
  evaluate: async () => {},
  evaluateExpressions: async () => [],
  resume: async () => {},
  getFrameScopes: async frame => frame.scope,
  getFrames: async () => [],
  setBreakpoint: () => new Promise(_resolve => {}),
  sourceContents: ({ source }) => {
    return new Promise((resolve, reject) => {
      switch (source) {
        case "foo1":
          return resolve({
            source: "function foo1() {\n  return 5;\n}",
            contentType: "text/javascript",
          });
        case "await":
          return resolve({
            source: "async function aWait() {\n await foo();  return 5;\n}",
            contentType: "text/javascript",
          });

        case "foo":
          return resolve({
            source: "function foo() {\n  return -5;\n}",
            contentType: "text/javascript",
          });
        case "foo-original":
          return resolve({
            source: "\n\nfunction fooOriginal() {\n  return -5;\n}",
            contentType: "text/javascript",
          });
        case "foo-wasm":
          return resolve({
            source: { binary: new ArrayBuffer(0) },
            contentType: "application/wasm",
          });
        case "foo-wasm/originalSource":
          return resolve({
            source: "fn fooBar() {}\nfn barZoo() { fooBar() }",
            contentType: "text/rust",
          });
      }

      return resolve();
    });
  },
  getSourceActorBreakpointPositions: async () => ({}),
  getSourceActorBreakableLines: async () => [],
  actorID: "threadActorID",
};

const mockFrameId = "1";

function createPauseInfo(
  frameLocation = { sourceId: "foo1", line: 2 },
  frameOpts = {}
) {
  const frames = [
    makeFrame(
      { id: mockFrameId, sourceId: frameLocation.sourceId },
      {
        location: frameLocation,
        generatedLocation: frameLocation,
        ...frameOpts,
      }
    ),
  ];
  return {
    thread: "FakeThread",
    frame: frames[0],
    frames,
    loadedObjects: [],
    why: makeWhyNormal(),
  };
}

describe("pause", () => {
  describe("stepping", () => {
    it("should set and clear the command", async () => {
      const { dispatch, getState } = createStore(mockCommandClient);
      const mockPauseInfo = createPauseInfo();

      await dispatch(actions.newGeneratedSource(makeSource("foo1")));
      await dispatch(actions.paused(mockPauseInfo));
      const cx = selectors.getThreadContext(getState());
      const stepped = dispatch(actions.stepIn(cx));
      expect(isStepping(getState(), "FakeThread")).toBeTruthy();
      if (!stepInResolve) {
        throw new Error("no stepInResolve");
      }
      await stepInResolve();
      await stepped;
      expect(isStepping(getState(), "FakeThread")).toBeFalsy();
    });

    it("should only step when paused", async () => {
      const client = { stepIn: jest.fn() };
      const { dispatch, cx } = createStore(client);

      dispatch(actions.stepIn(cx));
      expect(client.stepIn.mock.calls).toHaveLength(0);
    });

    it("should step when paused", async () => {
      const { dispatch, getState } = createStore(mockCommandClient);
      const mockPauseInfo = createPauseInfo();

      await dispatch(actions.newGeneratedSource(makeSource("foo1")));
      await dispatch(actions.paused(mockPauseInfo));
      const cx = selectors.getThreadContext(getState());
      dispatch(actions.stepIn(cx));
      expect(isStepping(getState(), "FakeThread")).toBeTruthy();
    });

    it("getting frame scopes with bindings", async () => {
      const generatedLocation = {
        sourceId: "foo",
        line: 1,
        column: 0,
      };

      const client = { ...mockCommandClient };
      const store = createStore(client, {});
      const { dispatch, getState } = store;
      const mockPauseInfo = createPauseInfo(generatedLocation, {
        scope: {
          bindings: {
            variables: { b: { value: {} } },
            arguments: [{ a: { value: {} } }],
          },
        },
      });

      const { frames } = mockPauseInfo;
      client.getFrames = async () => frames;

      const source = await dispatch(
        actions.newGeneratedSource(makeSource("foo"))
      );
      await dispatch(actions.newOriginalSource(makeOriginalSource(source)));

      await dispatch(actions.paused(mockPauseInfo));
      expect(selectors.getFrames(getState(), "FakeThread")).toEqual([
        {
          generatedLocation: { column: 0, line: 1, sourceId: "foo" },
          id: mockFrameId,
          location: { column: 0, line: 1, sourceId: "foo" },
          originalDisplayName: "foo",
          scope: {
            bindings: {
              arguments: [{ a: { value: {} } }],
              variables: { b: { value: {} } },
            },
          },
          thread: "FakeThread",
        },
      ]);

      expect(selectors.getFrameScopes(getState(), "FakeThread")).toEqual({
        generated: {
          "1": {
            pending: false,
            scope: {
              bindings: {
                arguments: [{ a: { value: {} } }],
                variables: { b: { value: {} } },
              },
            },
          },
        },
        mappings: { "1": undefined },
        original: { "1": { pending: false, scope: undefined } },
      });

      expect(
        selectors.getSelectedFrameBindings(getState(), "FakeThread")
      ).toEqual(["b", "a"]);
    });

    it("maps frame locations and names to original source", async () => {
      const generatedLocation = {
        sourceId: "foo",
        line: 1,
        column: 0,
      };

      const originalLocation = {
        sourceId: "foo-original",
        line: 3,
        column: 0,
      };

      const sourceMapsMock = {
        getOriginalLocation: () => Promise.resolve(originalLocation),
        getOriginalLocations: async items => items,
        getOriginalSourceText: async () => ({
          text: "\n\nfunction fooOriginal() {\n  return -5;\n}",
          contentType: "text/javascript",
        }),
        getGeneratedLocation: async location => location,
      };

      const client = { ...mockCommandClient };
      const store = createStore(client, {}, sourceMapsMock);
      const { dispatch, getState } = store;
      const mockPauseInfo = createPauseInfo(generatedLocation);

      const { frames } = mockPauseInfo;
      client.getFrames = async () => frames;

      await dispatch(actions.newGeneratedSource(makeSource("foo")));
      await dispatch(actions.newGeneratedSource(makeSource("foo-original")));

      await dispatch(actions.paused(mockPauseInfo));
      expect(selectors.getFrames(getState(), "FakeThread")).toEqual([
        {
          generatedLocation: { column: 0, line: 1, sourceId: "foo" },
          id: mockFrameId,
          location: { column: 0, line: 3, sourceId: "foo-original" },
          originalDisplayName: "fooOriginal",
          scope: { bindings: { arguments: [], variables: {} } },
          thread: "FakeThread",
        },
      ]);
    });

    it("maps frame to original frames", async () => {
      const generatedLocation = {
        sourceId: "foo-wasm",
        line: 1,
        column: 0,
      };

      const originalLocation = {
        sourceId: "foo-wasm/originalSource",
        line: 1,
        column: 1,
      };
      const originalLocation2 = {
        sourceId: "foo-wasm/originalSource",
        line: 2,
        column: 14,
      };

      const originStackFrames = [
        {
          displayName: "fooBar",
          thread: "FakeThread",
        },
        {
          displayName: "barZoo",
          location: originalLocation2,
          thread: "FakeThread",
        },
      ];

      const sourceMapsMock = {
        getOriginalStackFrames: loc => Promise.resolve(originStackFrames),
        getOriginalLocation: () => Promise.resolve(originalLocation),
        getOriginalLocations: async items => items,
        getOriginalSourceText: async () => ({
          text: "fn fooBar() {}\nfn barZoo() { fooBar() }",
          contentType: "text/rust",
        }),
        getGeneratedRangesForOriginal: async () => [],
      };

      const client = { ...mockCommandClient };
      const store = createStore(client, {}, sourceMapsMock);
      const { dispatch, getState } = store;
      const mockPauseInfo = createPauseInfo(generatedLocation);
      const { frames } = mockPauseInfo;
      client.getFrames = async () => frames;

      const source = await dispatch(
        actions.newGeneratedSource(
          makeSource("foo-wasm", { introductionType: "wasm" })
        )
      );
      await dispatch(actions.newOriginalSource(makeOriginalSource(source)));

      await dispatch(actions.paused(mockPauseInfo));
      expect(selectors.getFrames(getState(), "FakeThread")).toEqual([
        {
          asyncCause: undefined,
          displayName: "fooBar",
          generatedLocation: { column: 0, line: 1, sourceId: "foo-wasm" },
          id: "1",
          index: undefined,
          isOriginal: true,
          location: { column: 1, line: 1, sourceId: "foo-wasm/originalSource" },
          originalDisplayName: "fooBar",
          originalVariables: undefined,
          scope: { bindings: { arguments: [], variables: {} } },
          source: null,
          state: undefined,
          this: undefined,
          thread: "FakeThread",
        },
        {
          asyncCause: undefined,
          displayName: "barZoo",
          generatedLocation: { column: 0, line: 1, sourceId: "foo-wasm" },
          id: "1-originalFrame1",
          index: undefined,
          isOriginal: true,
          location: {
            column: 14,
            line: 2,
            sourceId: "foo-wasm/originalSource",
          },
          originalDisplayName: "barZoo",
          originalVariables: undefined,
          scope: { bindings: { arguments: [], variables: {} } },
          source: null,
          state: undefined,
          this: undefined,
          thread: "FakeThread",
        },
      ]);
    });
  });

  describe("resumed", () => {
    it("should not evaluate expression while stepping", async () => {
      const client = { ...mockCommandClient, evaluateExpressions: jest.fn() };
      const { dispatch, getState } = createStore(client);
      const mockPauseInfo = createPauseInfo();

      await dispatch(actions.newGeneratedSource(makeSource("foo1")));
      await dispatch(actions.paused(mockPauseInfo));

      const cx = selectors.getThreadContext(getState());
      dispatch(actions.stepIn(cx));
      await dispatch(actions.resumed(mockCommandClient.actorID));
      expect(client.evaluateExpressions.mock.calls).toHaveLength(1);
    });

    it("resuming - will re-evaluate watch expressions", async () => {
      const client = { ...mockCommandClient, evaluateExpressions: jest.fn() };
      const store = createStore(client);
      const { dispatch, getState, cx } = store;
      const mockPauseInfo = createPauseInfo();

      await dispatch(actions.newGeneratedSource(makeSource("foo1")));
      await dispatch(actions.newGeneratedSource(makeSource("foo")));
      await dispatch(actions.addExpression(cx, "foo"));
      await waitForState(store, state => selectors.getExpression(state, "foo"));

      client.evaluateExpressions.mockReturnValue(Promise.resolve(["YAY"]));
      await dispatch(actions.paused(mockPauseInfo));

      await dispatch(actions.resumed(mockCommandClient.actorID));
      const expression = selectors.getExpression(getState(), "foo");
      expect(expression && expression.value).toEqual("YAY");
    });
  });
});
