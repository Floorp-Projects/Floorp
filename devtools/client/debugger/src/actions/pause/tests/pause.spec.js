/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  actions,
  selectors,
  createStore,
  createSourceObject,
  makeSource,
  makeOriginalSource,
  makeFrame,
} from "../../../utils/test-head";

import { makeWhyNormal } from "../../../utils/test-mockup";
import { createLocation } from "../../../utils/location";

const mockCommandClient = {
  stepIn: () => new Promise(),
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
  frameLocation = createLocation({
    source: createSourceObject("foo1"),
    line: 2,
  }),
  frameOpts = {}
) {
  const frames = [
    makeFrame(
      { id: mockFrameId, sourceId: frameLocation.source.id },
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

function debuggerToSourceMapLocation(l) {
  return {
    sourceId: l.source.id,
    line: l.line,
    column: l.column,
  };
}

describe("pause", () => {
  describe("stepping", () => {
    it("should only step when paused", async () => {
      const client = { stepIn: jest.fn() };
      const { dispatch } = createStore(client);

      dispatch(actions.stepIn());
      expect(client.stepIn.mock.calls).toHaveLength(0);
    });

    it("getting frame scopes with bindings", async () => {
      const client = { ...mockCommandClient };
      const store = createStore(client, {});
      const { dispatch, getState } = store;

      const source = await dispatch(
        actions.newGeneratedSource(makeSource("foo"))
      );
      const generatedLocation = createLocation({
        source,
        line: 1,
        column: 0,
        sourceActor: selectors.getFirstSourceActorForGeneratedSource(
          getState(),
          source.id
        ),
      });
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
      await dispatch(actions.newOriginalSources([makeOriginalSource(source)]));

      await dispatch(actions.paused(mockPauseInfo));
      expect(selectors.getFrames(getState(), "FakeThread")).toEqual([
        {
          id: mockFrameId,
          generatedLocation,
          location: generatedLocation,
          library: null,
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
          1: {
            pending: false,
            scope: {
              bindings: {
                arguments: [{ a: { value: {} } }],
                variables: { b: { value: {} } },
              },
            },
          },
        },
        mappings: { 1: undefined },
        original: { 1: { pending: false, scope: undefined } },
      });

      expect(
        selectors.getSelectedFrameBindings(getState(), "FakeThread")
      ).toEqual(["b", "a"]);
    });

    it("maps frame to original frames", async () => {
      const sourceMapLoaderMock = {
        getOriginalStackFrames: loc => Promise.resolve(originStackFrames),
        getOriginalLocation: () =>
          Promise.resolve(debuggerToSourceMapLocation(originalLocation)),
        getOriginalLocations: async items =>
          items.map(debuggerToSourceMapLocation),
        getOriginalSourceText: async () => ({
          text: "fn fooBar() {}\nfn barZoo() { fooBar() }",
          contentType: "text/rust",
        }),
        getGeneratedRangesForOriginal: async () => [],
      };

      const client = { ...mockCommandClient };
      const store = createStore(client, {}, sourceMapLoaderMock);
      const { dispatch, getState } = store;

      const generatedSource = await dispatch(
        actions.newGeneratedSource(
          makeSource("foo-wasm", { introductionType: "wasm" })
        )
      );

      const generatedLocation = createLocation({
        source: generatedSource,
        line: 1,
        column: 0,
        sourceActor: selectors.getFirstSourceActorForGeneratedSource(
          getState(),
          generatedSource.id
        ),
      });
      const mockPauseInfo = createPauseInfo(generatedLocation);
      const { frames } = mockPauseInfo;
      client.getFrames = async () => frames;

      const [originalSource] = await dispatch(
        actions.newOriginalSources([makeOriginalSource(generatedSource)])
      );

      const originalLocation = createLocation({
        source: originalSource,
        line: 1,
        column: 1,
      });
      const originalLocation2 = createLocation({
        source: originalSource,
        line: 2,
        column: 14,
        sourceActor: selectors.getFirstSourceActorForGeneratedSource(
          getState(),
          originalSource.id
        ),
      });

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

      await dispatch(actions.paused(mockPauseInfo));
      expect(selectors.getFrames(getState(), "FakeThread")).toEqual([
        {
          asyncCause: undefined,
          displayName: "fooBar",
          generatedLocation,
          id: "1",
          index: undefined,
          isOriginal: true,
          library: null,
          location: originalLocation,
          originalDisplayName: "fooBar",
          originalVariables: undefined,
          state: undefined,
          this: undefined,
          thread: "FakeThread",
          type: undefined,
        },
        {
          asyncCause: undefined,
          displayName: "barZoo",
          generatedLocation,
          id: "1-originalFrame1",
          index: undefined,
          isOriginal: true,
          library: null,
          location: originalLocation2,
          originalDisplayName: "barZoo",
          originalVariables: undefined,
          state: undefined,
          this: undefined,
          thread: "FakeThread",
          type: undefined,
        },
      ]);
    });
  });
});
