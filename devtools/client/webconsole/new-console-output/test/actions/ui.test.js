/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const actions = require("devtools/client/webconsole/new-console-output/actions/index");
const {
  FILTER_BAR_TOGGLE
} = require("devtools/client/webconsole/new-console-output/constants");

const expect = require("expect");

describe("UI actions:", () => {
  describe("filterBarToggle", () => {
    it("creates expected action", () => {
      const action = actions.filterBarToggle();
      const expected = {
        type: FILTER_BAR_TOGGLE
      };

      expect(action).toEqual(expected);
    });
  });
});
