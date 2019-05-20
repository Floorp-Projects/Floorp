/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getCallStackFrames } from "../getCallStackFrames";
import { pullAt } from "lodash";
import { insertResources, createInitial } from "../../utils/resource";

describe("getCallStackFrames selector", () => {
  describe("library annotation", () => {
    it("annotates React frames", () => {
      const state = {
        frames: [
          { location: { sourceId: "source1" } },
          { location: { sourceId: "source2" } },
          { location: { sourceId: "source2" } },
        ],
        sources: insertResources(createInitial(), [
          { id: "source1", url: "webpack:///src/App.js" },
          {
            id: "source2",
            url:
              "webpack:///foo/node_modules/react-dom/lib/ReactCompositeComponent.js",
          },
        ]),
        selectedSource: {
          id: "sourceId-originalSource",
        },
      };

      const frames = getCallStackFrames.resultFunc(
        state.frames,
        state.sources,
        state.selectedSource,
        true
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
      const preAwaitGroup = [
        {
          displayName: "asyncAppFunction",
          location: { sourceId: "bundle" },
        },
        {
          displayName: "tryCatch",
          location: { sourceId: "regenerator" },
        },
        {
          displayName: "invoke",
          location: { sourceId: "regenerator" },
        },
        {
          displayName: "defineIteratorMethods/</prototype[method]",
          location: { sourceId: "regenerator" },
        },
        {
          displayName: "step",
          location: { sourceId: "bundle" },
        },
        {
          displayName: "_asyncToGenerator/</<",
          location: { sourceId: "bundle" },
        },
        {
          displayName: "Promise",
          location: { sourceId: "promise" },
        },
        {
          displayName: "_asyncToGenerator/<",
          location: { sourceId: "bundle" },
        },
        {
          displayName: "asyncAppFunction",
          location: { sourceId: "app" },
        },
      ];

      const postAwaitGroup = [
        {
          displayName: "asyncAppFunction",
          location: { sourceId: "bundle" },
        },
        {
          displayName: "tryCatch",
          location: { sourceId: "regenerator" },
        },
        {
          displayName: "invoke",
          location: { sourceId: "regenerator" },
        },
        {
          displayName: "defineIteratorMethods/</prototype[method]",
          location: { sourceId: "regenerator" },
        },
        {
          displayName: "step",
          location: { sourceId: "bundle" },
        },
        {
          displayName: "step/<",
          location: { sourceId: "bundle" },
        },
        {
          displayName: "run",
          location: { sourceId: "bundle" },
        },
        {
          displayName: "notify/<",
          location: { sourceId: "bundle" },
        },
        {
          displayName: "flush",
          location: { sourceId: "microtask" },
        },
      ];

      const state = {
        frames: [...preAwaitGroup, ...postAwaitGroup],
        sources: insertResources(createInitial(), [
          { id: "app", url: "webpack///app.js" },
          { id: "bundle", url: "https://foo.com/bundle.js" },
          {
            id: "regenerator",
            url: "webpack:///foo/node_modules/regenerator-runtime/runtime.js",
          },
          {
            id: "microtask",
            url: "webpack:///foo/node_modules/core-js/modules/_microtask.js",
          },
          {
            id: "promise",
            url: "webpack///foo/node_modules/core-js/modules/es6.promise.js",
          },
        ]),
        selectedSource: {
          id: "sourceId-originalSource",
        },
      };

      const frames = getCallStackFrames.resultFunc(
        state.frames,
        state.sources,
        state.selectedSource
      );

      const babelFrames = pullAt(frames, [
        1,
        2,
        3,
        4,
        5,
        6,
        7,
        8,
        10,
        11,
        12,
        13,
        14,
        15,
        16,
        17,
      ]);
      const otherFrames = frames;

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
