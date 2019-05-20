/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { reportException, executeSoon } from "../DevToolsUtils.js";

describe("DevToolsUtils", () => {
  describe("reportException", () => {
    beforeEach(() => {
      global.console = { error: jest.fn() };
    });

    it("calls console.error", () => {
      reportException("caller", ["you broke it"]);
      expect(console.error).toHaveBeenCalled();
    });

    it("returns a description of caller and exception text", () => {
      const who = "who",
        exception = "this is an error",
        msgTxt = " threw an exception: ";

      reportException(who, [exception]);

      expect(console.error).toHaveBeenCalledWith(`${who}${msgTxt}`, [
        exception,
      ]);
    });
  });

  describe("executeSoon", () => {
    it("calls setTimeout with 0 ms", () => {
      global.setTimeout = jest.fn();
      const fnc = () => {};

      executeSoon(fnc);

      expect(setTimeout).toHaveBeenCalledWith(fnc, 0);
    });
  });
});
