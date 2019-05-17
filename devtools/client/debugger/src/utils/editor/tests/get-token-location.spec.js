/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getTokenLocation } from "../get-token-location";

describe("getTokenLocation", () => {
  const codemirror = {
    coordsChar: jest.fn(() => ({
      line: 1,
      ch: "C",
    })),
  };
  const token: any = {
    getBoundingClientRect() {
      return {
        left: 10,
        top: 20,
        width: 10,
        height: 10,
      };
    },
  };
  it("calls into codeMirror", () => {
    getTokenLocation(codemirror, token);
    expect(codemirror.coordsChar).toHaveBeenCalledWith({
      left: 15,
      top: 25,
    });
  });
});
