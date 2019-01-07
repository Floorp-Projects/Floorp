/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getScopes } from "..";

describe("scopes", () => {
  describe("getScopes", () => {
    it("single scope", () => {
      const pauseData = {
        frame: {
          this: {}
        }
      };

      const selectedFrame = {
        scope: {
          actor: "actor1",
          type: "block",
          bindings: {
            arguments: [],
            variables: {}
          },
          parent: null
        },
        this: {}
      };

      const frameScopes = selectedFrame.scope;
      const scopes = getScopes(pauseData, selectedFrame, frameScopes);
      expect(scopes[0].path).toEqual("actor1-1");
      expect(scopes[0].contents[0]).toEqual({
        name: "<this>",
        path: "actor1-1/<this>",
        contents: { value: {} }
      });
    });

    it("second scope", () => {
      const pauseData = {
        frame: {
          this: {}
        }
      };

      const selectedFrame = {
        scope: {
          actor: "actor1",
          type: "block",
          bindings: {
            arguments: [],
            variables: {}
          },
          parent: {
            actor: "actor2",
            type: "block",
            bindings: {
              arguments: [],
              variables: {
                foo: {}
              }
            }
          }
        },
        this: {}
      };

      const frameScopes = selectedFrame.scope;
      const scopes = getScopes(pauseData, selectedFrame, frameScopes);
      expect(scopes[1].path).toEqual("actor2-2");
      expect(scopes[1].contents[0]).toEqual({
        name: "foo",
        path: "actor2-2/foo",
        contents: {}
      });
    });

    it("returning scope", () => {
      const why = {
        frameFinished: {
          return: "to sender"
        }
      };

      const selectedFrame = {
        scope: {
          actor: "actor1",
          type: "block",
          bindings: {
            arguments: [],
            variables: {}
          },
          parent: null
        },
        this: {}
      };

      const frameScopes = selectedFrame.scope;
      const scopes = getScopes(why, selectedFrame, frameScopes);
      expect(scopes).toMatchObject([
        {
          path: "actor1-1",
          contents: [
            {
              name: "<return>",
              path: "actor1-1/<return>",
              contents: {
                value: "to sender"
              }
            },
            {
              name: "<this>",
              path: "actor1-1/<this>",
              contents: {
                value: {}
              }
            }
          ]
        }
      ]);
    });

    it("throwing scope", () => {
      const why = {
        frameFinished: {
          throw: "a party"
        }
      };

      const selectedFrame = {
        scope: {
          actor: "actor1",
          type: "block",
          bindings: {
            arguments: [],
            variables: {}
          },
          parent: null
        },
        this: {}
      };

      const frameScopes = selectedFrame.scope;
      const scopes = getScopes(why, selectedFrame, frameScopes);
      expect(scopes).toMatchObject([
        {
          path: "actor1-1",
          contents: [
            {
              name: "<exception>",
              path: "actor1-1/<exception>",
              contents: {
                value: "a party"
              }
            },
            {
              name: "<this>",
              path: "actor1-1/<this>",
              contents: {
                value: {}
              }
            }
          ]
        }
      ]);
    });
  });
});
