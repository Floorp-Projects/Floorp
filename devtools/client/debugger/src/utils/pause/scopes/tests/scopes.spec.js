/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getScopes } from "..";
import {
  makeMockFrame,
  makeMockScope,
  makeWhyNormal,
  makeWhyThrow,
  mockScopeAddVariable,
} from "../../../test-mockup";

import type { Scope } from "../../../../types";
import type { RenderableScope } from "../getScope";

function convertScope(scope: Scope): RenderableScope {
  return (scope: any);
}

describe("scopes", () => {
  describe("getScopes", () => {
    it("single scope", () => {
      const pauseData = makeWhyNormal();
      const scope = makeMockScope("actor1");
      const selectedFrame = makeMockFrame(undefined, undefined, scope);

      const frameScopes = convertScope(selectedFrame.scope);
      const scopes = getScopes(pauseData, selectedFrame, frameScopes);
      if (!scopes) {
        throw new Error("missing scopes");
      }
      expect(scopes[0].path).toEqual("actor1-1");
      expect(scopes[0].contents[0]).toEqual({
        name: "<this>",
        path: "actor1-1/<this>",
        contents: { value: {} },
      });
    });

    it("second scope", () => {
      const pauseData = makeWhyNormal();
      const scope0 = makeMockScope("actor2");
      const scope1 = makeMockScope("actor1", undefined, scope0);
      const selectedFrame = makeMockFrame(undefined, undefined, scope1);
      mockScopeAddVariable(scope0, "foo");

      const frameScopes = convertScope(selectedFrame.scope);
      const scopes = getScopes(pauseData, selectedFrame, frameScopes);
      if (!scopes) {
        throw new Error("missing scopes");
      }
      expect(scopes[1].path).toEqual("actor2-2");
      expect(scopes[1].contents[0]).toEqual({
        name: "foo",
        path: "actor2-2/foo",
        contents: { value: null },
      });
    });

    it("returning scope", () => {
      const why = makeWhyNormal("to sender");
      const scope = makeMockScope("actor1");
      const selectedFrame = makeMockFrame(undefined, undefined, scope);

      const frameScopes = convertScope(selectedFrame.scope);
      const scopes = getScopes(why, selectedFrame, frameScopes);
      expect(scopes).toMatchObject([
        {
          path: "actor1-1",
          contents: [
            {
              name: "<return>",
              path: "actor1-1/<return>",
              contents: {
                value: "to sender",
              },
            },
            {
              name: "<this>",
              path: "actor1-1/<this>",
              contents: {
                value: {},
              },
            },
          ],
        },
      ]);
    });

    it("throwing scope", () => {
      const why = makeWhyThrow("a party");
      const scope = makeMockScope("actor1");
      const selectedFrame = makeMockFrame(undefined, undefined, scope);

      const frameScopes = convertScope(selectedFrame.scope);
      const scopes = getScopes(why, selectedFrame, frameScopes);
      expect(scopes).toMatchObject([
        {
          path: "actor1-1",
          contents: [
            {
              name: "<exception>",
              path: "actor1-1/<exception>",
              contents: {
                value: "a party",
              },
            },
            {
              name: "<this>",
              path: "actor1-1/<this>",
              contents: {
                value: {},
              },
            },
          ],
        },
      ]);
    });
  });
});
