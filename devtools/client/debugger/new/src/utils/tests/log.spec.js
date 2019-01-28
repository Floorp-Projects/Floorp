/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { prefs } from "../prefs";
import { log } from "../log.js";

let logArgFirst, logArgSecond, logArgArray;

describe("log()", () => {
  beforeEach(() => {
    logArgFirst = "my info";
    logArgSecond = "my other info";
    logArgArray = [logArgFirst, logArgSecond];
    global.console = { log: jest.fn() };
  });

  afterEach(() => {
    prefs.logging = false;
  });

  describe("when isDevelopment", () => {
    it("prints arguments", () => {
      prefs.logging = true;
      log(logArgArray);

      expect(global.console.log).toHaveBeenCalledWith(logArgArray);
    });

    it("does not print by default", () => {
      log(logArgArray);
      expect(global.console.log).not.toHaveBeenCalled();
    });
  });
});
