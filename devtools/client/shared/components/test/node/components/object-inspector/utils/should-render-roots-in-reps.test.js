/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const Utils = require("devtools/client/shared/components/object-inspector/utils/index");
const { shouldRenderRootsInReps } = Utils;

const nullStubs = require("devtools/client/shared/components/test/node/stubs/reps/null");
const numberStubs = require("devtools/client/shared/components/test/node/stubs/reps/number");
const undefinedStubs = require("devtools/client/shared/components/test/node/stubs/reps/undefined");
const gripStubs = require("devtools/client/shared/components/test/node/stubs/reps/grip");
const gripArrayStubs = require("devtools/client/shared/components/test/node/stubs/reps/grip-array");
const symbolStubs = require("devtools/client/shared/components/test/node/stubs/reps/symbol");
const errorStubs = require("devtools/client/shared/components/test/node/stubs/reps/error");
const bigIntStubs = require("devtools/client/shared/components/test/node/stubs/reps/big-int");

describe("shouldRenderRootsInReps", () => {
  it("returns true for a string", () => {
    expect(
      shouldRenderRootsInReps([
        {
          contents: { value: "Hello" },
        },
      ])
    ).toBeTruthy();
  });

  it("returns true for an integer", () => {
    expect(
      shouldRenderRootsInReps([
        {
          contents: { value: numberStubs.get("Int") },
        },
      ])
    ).toBeTruthy();
  });

  it("returns false for empty roots", () => {
    expect(shouldRenderRootsInReps([])).toBeFalsy();
  });

  it("returns true for a big int", () => {
    expect(
      shouldRenderRootsInReps([
        {
          contents: { value: bigIntStubs.get("1n") },
        },
      ])
    ).toBeTruthy();
  });

  it("returns true for undefined", () => {
    expect(
      shouldRenderRootsInReps([
        {
          contents: { value: undefinedStubs.get("Undefined") },
        },
      ])
    ).toBeTruthy();
  });

  it("returns true for null", () => {
    expect(
      shouldRenderRootsInReps([
        {
          contents: { value: nullStubs.get("Null") },
        },
      ])
    ).toBeTruthy();
  });

  it("returns true for Symbols", () => {
    expect(
      shouldRenderRootsInReps([
        {
          contents: { value: symbolStubs.get("Symbol") },
        },
      ])
    ).toBeTruthy();
  });

  it("returns true for Errors when customFormat prop is true", () => {
    expect(
      shouldRenderRootsInReps(
        [
          {
            contents: { value: errorStubs.get("MultilineStackError") },
          },
        ],
        { customFormat: true }
      )
    ).toBeTruthy();
  });

  it("returns false for Errors when customFormat prop is false", () => {
    expect(
      shouldRenderRootsInReps(
        [
          {
            contents: { value: errorStubs.get("MultilineStackError") },
          },
        ],
        { customFormat: false }
      )
    ).toBeFalsy();
  });

  it("returns false when there are multiple primitive roots", () => {
    expect(
      shouldRenderRootsInReps([
        {
          contents: { value: "Hello" },
        },
        {
          contents: { value: 42 },
        },
      ])
    ).toBeFalsy();
  });

  it("returns false for primitive when the root specifies a name", () => {
    expect(
      shouldRenderRootsInReps([
        {
          name: "label",
          contents: { value: 42 },
        },
      ])
    ).toBeFalsy();
  });

  it("returns false for Grips", () => {
    expect(
      shouldRenderRootsInReps([
        {
          name: "label",
          contents: { value: gripStubs.get("testMaxProps") },
        },
      ])
    ).toBeFalsy();
  });

  it("returns false for Arrays", () => {
    expect(
      shouldRenderRootsInReps([
        {
          name: "label",
          contents: { value: gripArrayStubs.get("testMaxProps") },
        },
      ])
    ).toBeFalsy();
  });
});
