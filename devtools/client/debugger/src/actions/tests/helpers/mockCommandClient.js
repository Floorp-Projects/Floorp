/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { SourceActor } from "../../../types";

export function createSource(name: string, code?: string) {
  name = name.replace(/\..*$/, "");
  return {
    source: code || `function ${name}() {\n  return ${name} \n}`,
    contentType: "text/javascript",
  };
}

const sources = [
  "a",
  "b",
  "foo",
  "bar",
  "foo1",
  "foo2",
  "a.js",
  "baz.js",
  "foobar.js",
  "barfoo.js",
  "foo.js",
  "bar.js",
  "base.js",
  "bazz.js",
  "jquery.js",
];

export const mockCommandClient = {
  sourceContents: function({
    source,
  }: SourceActor): Promise<{| source: any, contentType: ?string |}> {
    return new Promise((resolve, reject) => {
      if (sources.includes(source)) {
        resolve(createSource(source));
      }

      reject(`unknown source: ${source}`);
    });
  },
  setBreakpoint: async () => {},
  removeBreakpoint: (_id: string) => Promise.resolve(),
  threadFront: async () => {},
  getFrameScopes: async () => {},
  evaluateExpressions: async () => {},
  getSourceActorBreakpointPositions: async () => ({}),
  getSourceActorBreakableLines: async () => [],
};
