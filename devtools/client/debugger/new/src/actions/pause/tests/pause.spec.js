/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  actions,
  selectors,
  createStore,
  waitForState,
  makeSource,
  makeOriginalSource,
  makeFrame
} from "../../../utils/test-head";

import { makeWhyNormal } from "../../../utils/test-mockup";

import * as parser from "../../../workers/parser/index";

const { isStepping } = selectors;

let stepInResolve = null;
const mockThreadClient = {
  stepIn: () =>
    new Promise(_resolve => {
      stepInResolve = _resolve;
    }),
  stepOver: () => new Promise(_resolve => _resolve),
  evaluate: async () => {},
  evaluateInFrame: async () => {},
  evaluateExpressions: async () => {},
  resume: async () => {},
  getFrameScopes: async frame => frame.scope,
  setBreakpoint: () => new Promise(_resolve => {}),
  sourceContents: ({ source }) => {
    return new Promise((resolve, reject) => {
      switch (source) {
        case "foo1":
          return resolve({
            source: "function foo1() {\n  return 5;\n}",
            contentType: "text/javascript"
          });
        case "await":
          return resolve({
            source: "async function aWait() {\n await foo();  return 5;\n}",
            contentType: "text/javascript"
          });

        case "foo":
          return resolve({
            source: "function foo() {\n  return -5;\n}",
            contentType: "text/javascript"
          });
        case "foo-original":
          return resolve({
            source: "\n\nfunction fooOriginal() {\n  return -5;\n}",
            contentType: "text/javascript"
          });
        case "foo-wasm":
          return resolve({
            source: { binary: new ArrayBuffer(0) },
            contentType: "application/wasm"
          });
        case "foo-wasm/originalSource":
          return resolve({
            source: "fn fooBar() {}\nfn barZoo() { fooBar() }",
            contentType: "text/rust"
          });
      }
    });
  },
  getBreakpointPositions: async () => ({})
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
        ...frameOpts
      }
    )
  ];
  return {
    thread: "FakeThread",
    frame: frames[0],
    frames,
    loadedObjects: [],
    why: makeWhyNormal()
  };
}

function resumedPacket() {
  return { from: "FakeThread", type: "resumed" };
}

describe("pause", () => {
  describe("stepping", () => {
    it("should set and clear the command", async () => {
      const { dispatch, getState } = createStore(mockThreadClient);
      const mockPauseInfo = createPauseInfo();

      await dispatch(actions.newSource(makeSource("foo1")));
      await dispatch(actions.paused(mockPauseInfo));
      const stepped = dispatch(actions.stepIn());
      expect(isStepping(getState())).toBeTruthy();
      if (!stepInResolve) {
        throw new Error("no stepInResolve");
      }
      await stepInResolve();
      await stepped;
      expect(isStepping(getState())).toBeFalsy();
    });

    it("should only step when paused", async () => {
      const client = { stepIn: jest.fn() };
      const { dispatch } = createStore(client);

      dispatch(actions.stepIn());
      expect(client.stepIn.mock.calls).toHaveLength(0);
    });

    it("should step when paused", async () => {
      const { dispatch, getState } = createStore(mockThreadClient);
      const mockPauseInfo = createPauseInfo();

      await dispatch(actions.newSource(makeSource("foo1")));
      await dispatch(actions.paused(mockPauseInfo));
      dispatch(actions.stepIn());
      expect(isStepping(getState())).toBeTruthy();
    });

    it("should step over when paused", async () => {
      const store = createStore(mockThreadClient);
      const { dispatch, getState } = store;
      const mockPauseInfo = createPauseInfo();

      await dispatch(actions.newSource(makeSource("foo1")));
      await dispatch(actions.paused(mockPauseInfo));
      const getNextStepSpy = jest.spyOn(parser, "getNextStep");
      dispatch(actions.stepOver());
      expect(getNextStepSpy).not.toBeCalled();
      expect(isStepping(getState())).toBeTruthy();
    });

    it("should step over when paused before an await", async () => {
      const store = createStore(mockThreadClient);
      const { dispatch } = store;
      const mockPauseInfo = createPauseInfo({
        sourceId: "await",
        line: 2,
        column: 0
      });

      const source = makeSource("await");
      await dispatch(actions.newSource(source));
      await dispatch(actions.loadSourceText(source));

      await dispatch(actions.paused(mockPauseInfo));
      const getNextStepSpy = jest.spyOn(parser, "getNextStep");
      dispatch(actions.stepOver());
      expect(getNextStepSpy).toBeCalled();
      getNextStepSpy.mockRestore();
    });

    it("should step over when paused after an await", async () => {
      const store = createStore(mockThreadClient);
      const { dispatch } = store;
      const mockPauseInfo = createPauseInfo({
        sourceId: "await",
        line: 2,
        column: 6
      });

      const source = makeSource("await");
      await dispatch(actions.newSource(source));
      await dispatch(actions.loadSourceText(source));

      await dispatch(actions.paused(mockPauseInfo));
      const getNextStepSpy = jest.spyOn(parser, "getNextStep");
      dispatch(actions.stepOver());
      expect(getNextStepSpy).toBeCalled();
      getNextStepSpy.mockRestore();
    });

    it("getting frame scopes with bindings", async () => {
      const generatedLocation = {
        sourceId: "foo",
        line: 1,
        column: 0
      };

      const store = createStore(mockThreadClient, {});
      const { dispatch, getState } = store;
      const mockPauseInfo = createPauseInfo(generatedLocation, {
        scope: {
          bindings: { variables: { b: {} }, arguments: [{ a: {} }] }
        }
      });

      const source = makeSource("foo");
      await dispatch(actions.newSource(source));
      await dispatch(actions.newSource(makeOriginalSource("foo")));
      await dispatch(actions.loadSourceText(source));

      await dispatch(actions.paused(mockPauseInfo));
      expect(selectors.getFrames(getState())).toEqual([
        {
          generatedLocation: { column: 0, line: 1, sourceId: "foo" },
          id: mockFrameId,
          location: { column: 0, line: 1, sourceId: "foo" },
          scope: {
            bindings: { arguments: [{ a: {} }], variables: { b: {} } }
          }
        }
      ]);

      expect(selectors.getFrameScopes(getState())).toEqual({
        generated: {
          "1": {
            pending: false,
            scope: {
              bindings: { arguments: [{ a: {} }], variables: { b: {} } }
            }
          }
        },
        mappings: { "1": null },
        original: { "1": { pending: false, scope: null } }
      });

      expect(selectors.getSelectedFrameBindings(getState())).toEqual([
        "b",
        "a"
      ]);
    });

    it("maps frame locations and names to original source", async () => {
      const generatedLocation = {
        sourceId: "foo",
        line: 1,
        column: 0
      };

      const originalLocation = {
        sourceId: "foo-original",
        line: 3,
        column: 0
      };

      const sourceMapsMock = {
        getOriginalLocation: () => Promise.resolve(originalLocation),
        getOriginalLocations: async items => items,
        getOriginalSourceText: async () => ({
          source: "\n\nfunction fooOriginal() {\n  return -5;\n}",
          contentType: "text/javascript"
        }),
        getGeneratedLocation: async location => location
      };

      const store = createStore(mockThreadClient, {}, sourceMapsMock);
      const { dispatch, getState } = store;
      const mockPauseInfo = createPauseInfo(generatedLocation);

      const fooSource = makeSource("foo");
      const fooOriginalSource = makeSource("foo-original");
      await dispatch(actions.newSource(fooSource));
      await dispatch(actions.newSource(fooOriginalSource));
      await dispatch(actions.loadSourceText(fooSource));
      await dispatch(actions.loadSourceText(fooOriginalSource));
      await dispatch(actions.setSymbols("foo-original"));

      await dispatch(actions.paused(mockPauseInfo));
      expect(selectors.getFrames(getState())).toEqual([
        {
          generatedLocation: { column: 0, line: 1, sourceId: "foo" },
          id: mockFrameId,
          location: { column: 0, line: 3, sourceId: "foo-original" },
          originalDisplayName: "fooOriginal",
          scope: { bindings: { arguments: [], variables: {} } }
        }
      ]);
    });

    it("maps frame to original frames", async () => {
      const generatedLocation = {
        sourceId: "foo-wasm",
        line: 1,
        column: 0
      };

      const originalLocation = {
        sourceId: "foo-wasm/originalSource",
        line: 1,
        column: 1
      };
      const originalLocation2 = {
        sourceId: "foo-wasm/originalSource",
        line: 2,
        column: 14
      };

      const originStackFrames = [
        {
          displayName: "fooBar"
        },
        {
          displayName: "barZoo",
          location: originalLocation2
        }
      ];

      const sourceMapsMock = {
        getOriginalStackFrames: loc => Promise.resolve(originStackFrames),
        getOriginalLocation: () => Promise.resolve(originalLocation),
        getOriginalLocations: async items => items,
        getOriginalSourceText: async () => ({
          source: "fn fooBar() {}\nfn barZoo() { fooBar() }",
          contentType: "text/rust"
        }),
        getGeneratedRangesForOriginal: async () => []
      };

      const store = createStore(mockThreadClient, {}, sourceMapsMock);
      const { dispatch, getState } = store;
      const mockPauseInfo = createPauseInfo(generatedLocation);

      const source = makeSource("foo-wasm", { isWasm: true });
      const originalSource = makeOriginalSource("foo-wasm");

      await dispatch(actions.newSource(source));
      await dispatch(actions.newSource(originalSource));
      await dispatch(actions.loadSourceText(source));
      await dispatch(actions.loadSourceText(originalSource));

      await dispatch(actions.paused(mockPauseInfo));
      expect(selectors.getFrames(getState())).toEqual([
        {
          displayName: "fooBar",
          generatedLocation: { column: 0, line: 1, sourceId: "foo-wasm" },
          id: mockFrameId,
          isOriginal: true,
          location: originalLocation,
          originalDisplayName: "fooBar",
          scope: { bindings: { arguments: [], variables: {} } },
          source: null,
          this: undefined,
          thread: undefined
        },
        {
          displayName: "barZoo",
          generatedLocation: { column: 0, line: 1, sourceId: "foo-wasm" },
          id: `${mockFrameId}-originalFrame1`,
          isOriginal: true,
          location: originalLocation2,
          originalDisplayName: "barZoo",
          scope: { bindings: { arguments: [], variables: {} } },
          source: null,
          this: undefined,
          thread: undefined
        }
      ]);
    });
  });

  describe("resumed", () => {
    it("should not evaluate expression while stepping", async () => {
      const client = { evaluateExpressions: jest.fn() };
      const { dispatch } = createStore(client);

      dispatch(actions.stepIn());
      await dispatch(actions.resumed(resumedPacket()));
      expect(client.evaluateExpressions.mock.calls).toHaveLength(1);
    });

    it("resuming - will re-evaluate watch expressions", async () => {
      const store = createStore(mockThreadClient);
      const { dispatch, getState } = store;
      const mockPauseInfo = createPauseInfo();

      await dispatch(actions.newSource(makeSource("foo1")));
      await dispatch(actions.newSource(makeSource("foo")));
      dispatch(actions.addExpression("foo"));
      await waitForState(store, state => selectors.getExpression(state, "foo"));

      mockThreadClient.evaluateExpressions = () => new Promise(r => r(["YAY"]));
      await dispatch(actions.paused(mockPauseInfo));

      await dispatch(actions.resumed(resumedPacket()));
      const expression = selectors.getExpression(getState(), "foo");
      expect(expression.value).toEqual("YAY");
    });
  });
});
