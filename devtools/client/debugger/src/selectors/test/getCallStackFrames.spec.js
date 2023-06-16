/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getCallStackFrames } from "../getCallStackFrames";

describe("getCallStackFrames selector", () => {
  describe("library annotation", () => {
    it("annotates React frames", () => {
      const source1 = { id: "source1", url: "webpack:///src/App.js" };
      const source2 = {
        id: "source2",
        url: "webpack:///foo/node_modules/react-dom/lib/ReactCompositeComponent.js",
      };
      const state = {
        frames: [
          { location: { sourceId: "source1", source: source1 } },
          { location: { sourceId: "source2", source: source2 } },
          { location: { sourceId: "source2", source: source2 } },
        ],
        selectedSource: {
          id: "sourceId-originalSource",
          isOriginal: true,
        },
      };

      const frames = getCallStackFrames.resultFunc(
        state.frames,
        state.selectedSource,
        {}
      );

      expect(frames[0]).not.toHaveProperty("library");
      expect(frames[1]).toHaveProperty("library", "React");
      expect(frames[2]).toHaveProperty("library", "React");
    });

    // Multiple Babel async frame groups occur when you have an async function
    // calling another async function (a common case).
    //
    // There are two possible frame groups that can occur depending on whether
    // one sets a breakpoint before or after an await
    it("annotates frames related to Babel async transforms", () => {
      const appSource = { id: "app", url: "webpack///app.js" };
      const bundleSource = { id: "bundle", url: "https://foo.com/bundle.js" };
      const regeneratorSource = {
        id: "regenerator",
        url: "webpack:///foo/node_modules/regenerator-runtime/runtime.js",
      };
      const microtaskSource = {
        id: "microtask",
        url: "webpack:///foo/node_modules/core-js/modules/_microtask.js",
      };
      const promiseSource = {
        id: "promise",
        url: "webpack///foo/node_modules/core-js/modules/es6.promise.js",
      };
      const preAwaitGroup = [
        {
          displayName: "asyncAppFunction",
          location: { source: bundleSource },
        },
        {
          displayName: "tryCatch",
          location: { source: regeneratorSource },
        },
        {
          displayName: "invoke",
          location: { source: regeneratorSource },
        },
        {
          displayName: "defineIteratorMethods/</prototype[method]",
          location: { source: regeneratorSource },
        },
        {
          displayName: "step",
          location: { source: bundleSource },
        },
        {
          displayName: "_asyncToGenerator/</<",
          location: { source: bundleSource },
        },
        {
          displayName: "Promise",
          location: { source: promiseSource },
        },
        {
          displayName: "_asyncToGenerator/<",
          location: { source: bundleSource },
        },
        {
          displayName: "asyncAppFunction",
          location: { source: appSource },
        },
      ];

      const postAwaitGroup = [
        {
          displayName: "asyncAppFunction",
          location: { source: bundleSource },
        },
        {
          displayName: "tryCatch",
          location: { source: regeneratorSource },
        },
        {
          displayName: "invoke",
          location: { source: regeneratorSource },
        },
        {
          displayName: "defineIteratorMethods/</prototype[method]",
          location: { source: regeneratorSource },
        },
        {
          displayName: "step",
          location: { source: bundleSource },
        },
        {
          displayName: "step/<",
          location: { source: bundleSource },
        },
        {
          displayName: "run",
          location: { source: bundleSource },
        },
        {
          displayName: "notify/<",
          location: { source: bundleSource },
        },
        {
          displayName: "flush",
          location: { source: microtaskSource },
        },
      ];

      const state = {
        frames: [...preAwaitGroup, ...postAwaitGroup],
        selectedSource: {
          id: "sourceId-originalSource",
          isOriginal: true,
        },
      };

      const frames = getCallStackFrames.resultFunc(
        state.frames,
        state.selectedSource,
        {}
      );

      // frames from 1-8 and 10-17 are babel frames.
      const babelFrames = [...frames.slice(1, 7), ...frames.slice(10, 7)];
      const otherFrames = frames.filter(frame => !babelFrames.includes(frame));

      expect(babelFrames).toEqual(
        Array(babelFrames.length).fill(
          expect.objectContaining({ library: "Babel" })
        )
      );
      expect(otherFrames).not.toEqual(
        Array(babelFrames.length).fill(
          expect.objectContaining({ library: "Babel" })
        )
      );
    });
  });
});
