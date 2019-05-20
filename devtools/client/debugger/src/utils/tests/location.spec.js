/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { sortSelectedLocations } from "../location";

function loc(line, column) {
  return {
    location: { sourceId: "foo", line, column },
    generatedLocation: { sourceId: "foo", line, column },
  };
}
describe("location.spec.js", () => {
  it("sorts lines", () => {
    const a = loc(3, 5);
    const b = loc(1, 10);
    expect(sortSelectedLocations([a, b], { id: "foo" })).toEqual([b, a]);
  });

  it("sorts columns", () => {
    const a = loc(3, 10);
    const b = loc(3, 5);
    expect(sortSelectedLocations([a, b], { id: "foo" })).toEqual([b, a]);
  });

  it("prioritizes undefined columns", () => {
    const a = loc(3, 10);
    const b = loc(3, undefined);
    expect(sortSelectedLocations([a, b], { id: "foo" })).toEqual([b, a]);
  });
});
