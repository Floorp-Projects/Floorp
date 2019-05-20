/* eslint max-nested-callbacks: ["error", 4] */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getFramePopVariables } from "../utils";
import type { NamedValue } from "../types";

const errorGrip = {
  type: "object",
  actor: "server2.conn66.child1/pausedobj243",
  class: "Error",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 4,
  preview: {
    kind: "Error",
    name: "Error",
    message: "blah",
    stack:
      "onclick@http://localhost:8000/examples/doc-return-values.html:1:18\n",
    fileName: "http://localhost:8000/examples/doc-return-values.html",
    lineNumber: 1,
    columnNumber: 18,
  },
};

function returnWhy(grip) {
  return {
    type: "resumeLimit",
    frameFinished: {
      return: grip,
    },
  };
}

function throwWhy(grip) {
  return {
    type: "resumeLimit",
    frameFinished: {
      throw: grip,
    },
  };
}

function getContentsValue(v: NamedValue) {
  return (v.contents: any).value;
}

function getContentsClass(v: NamedValue) {
  const value = getContentsValue(v);
  return value ? value.class || undefined : "";
}

describe("pause - scopes", () => {
  describe("getFramePopVariables", () => {
    describe("falsey values", () => {
      // NOTE: null and undefined are treated like objects and given a type
      const falsey = { false: false, "0": 0, null: { type: "null" } };
      for (const test in falsey) {
        const value = falsey[test];
        it(`shows ${test} returns`, () => {
          const why = returnWhy(value);
          const vars = getFramePopVariables(why, "");
          expect(vars[0].name).toEqual("<return>");
          expect(vars[0].name).toEqual("<return>");
          expect(getContentsValue(vars[0])).toEqual(value);
        });

        it(`shows ${test} throws`, () => {
          const why = throwWhy(value);
          const vars = getFramePopVariables(why, "");
          expect(vars[0].name).toEqual("<exception>");
          expect(vars[0].name).toEqual("<exception>");
          expect(getContentsValue(vars[0])).toEqual(value);
        });
      }
    });

    describe("Error / Objects", () => {
      it("shows Error returns", () => {
        const why = returnWhy(errorGrip);
        const vars = getFramePopVariables(why, "");
        expect(vars[0].name).toEqual("<return>");
        expect(vars[0].name).toEqual("<return>");
        expect(getContentsClass(vars[0])).toEqual("Error");
      });

      it("shows error throws", () => {
        const why = throwWhy(errorGrip);
        const vars = getFramePopVariables(why, "");
        expect(vars[0].name).toEqual("<exception>");
        expect(vars[0].name).toEqual("<exception>");
        expect(getContentsClass(vars[0])).toEqual("Error");
      });
    });

    describe("undefined", () => {
      it("does not show undefined returns", () => {
        const why = returnWhy({ type: "undefined" });
        const vars = getFramePopVariables(why, "");
        expect(vars).toHaveLength(0);
      });

      it("shows undefined throws", () => {
        const why = throwWhy({ type: "undefined" });
        const vars = getFramePopVariables(why, "");
        expect(vars[0].name).toEqual("<exception>");
        expect(vars[0].name).toEqual("<exception>");
        expect(getContentsValue(vars[0])).toEqual({ type: "undefined" });
      });
    });
  });
});
