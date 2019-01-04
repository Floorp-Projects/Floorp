/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { actions, createStore } from "../../utils/test-head";
const threadClient = {
  evaluate: jest.fn()
};

describe("toolbox", () => {
  describe("evaluate in console", () => {
    it("variable", () => {
      const { dispatch } = createStore(threadClient);
      dispatch(actions.evaluateInConsole("foo"));

      expect(threadClient.evaluate).toBeCalledWith(
        'console.log("foo"); console.log(foo)',
        { frameId: null }
      );
    });
  });
});
