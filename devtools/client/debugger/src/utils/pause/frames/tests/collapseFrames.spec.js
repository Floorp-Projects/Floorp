/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { collapseFrames } from "../collapseFrames";

describe("collapseFrames", () => {
  it("default", () => {
    const groups = collapseFrames([
      { displayName: "a", location: { source: {} } },

      { displayName: "b", library: "React", location: { source: {} } },
      { displayName: "c", library: "React", location: { source: {} } },
    ]);

    expect(groups).toMatchSnapshot();
  });

  it("promises", () => {
    const groups = collapseFrames([
      { displayName: "a", location: { source: {} } },

      { displayName: "b", library: "React", location: { source: {} } },
      { displayName: "c", library: "React", location: { source: {} } },
      {
        displayName: "d",
        library: undefined,
        asyncCause: "promise callback",
        location: { source: {} },
      },
      { displayName: "e", library: "React", location: { source: {} } },
      { displayName: "f", library: "React", location: { source: {} } },
      {
        displayName: "g",
        library: undefined,
        asyncCause: null,
        location: { source: {} },
      },
    ]);

    expect(groups).toMatchSnapshot();
  });
});
