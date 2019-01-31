/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getNextStep } from "../steps";
import { getSource } from "./helpers";
import { setSource } from "../sources";

describe("getNextStep", () => {
  describe("await", () => {
    it("first await call", () => {
      const source = getSource("async");
      setSource(source);
      const pausePosition = { line: 8, column: 2, sourceId: "async" };
      expect(getNextStep("async", pausePosition)).toEqual({
        ...pausePosition,
        line: 9
      });
    });

    it("first await call expression", () => {
      const source = getSource("async");
      setSource(source);
      const pausePosition = { line: 8, column: 9, sourceId: "async" };
      expect(getNextStep("async", pausePosition)).toEqual({
        ...pausePosition,
        line: 9,
        column: 2
      });
    });

    it("second await call", () => {
      const source = getSource("async");
      setSource(source);
      const pausePosition = { line: 9, column: 2, sourceId: "async" };
      expect(getNextStep("async", pausePosition)).toEqual(null);
    });

    it("second call expression", () => {
      const source = getSource("async");
      setSource(source);
      const pausePosition = { line: 9, column: 9, sourceId: "async" };
      expect(getNextStep("async", pausePosition)).toEqual(null);
    });
  });

  describe("yield", () => {
    it("first yield call", () => {
      const source = getSource("generators");
      setSource(source);
      const pausePosition = { line: 2, column: 2, sourceId: "generators" };
      expect(getNextStep("generators", pausePosition)).toEqual({
        ...pausePosition,
        line: 3
      });
    });

    it("second yield call", () => {
      const source = getSource("generators");
      setSource(source);
      const pausePosition = { line: 3, column: 2, sourceId: "generators" };
      expect(getNextStep("generators", pausePosition)).toEqual(null);
    });
  });
});
