/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { collapseFrames } from "../collapseFrames";

describe("collapseFrames", () => {
  it("default", () => {
    const groups = collapseFrames([
      { displayName: "a" },

      { displayName: "b", library: "React" },
      { displayName: "c", library: "React" },
    ]);

    expect(groups).toMatchSnapshot();
  });

  it("promises", () => {
    const groups = collapseFrames([
      { displayName: "a" },

      { displayName: "b", library: "React" },
      { displayName: "c", library: "React" },
      {
        displayName: "d",
        library: undefined,
        asyncCause: "promise callback",
      },
      { displayName: "e", library: "React" },
      { displayName: "f", library: "React" },
      { displayName: "g", library: undefined, asyncCause: null },
    ]);

    expect(groups).toMatchSnapshot();
  });
});
