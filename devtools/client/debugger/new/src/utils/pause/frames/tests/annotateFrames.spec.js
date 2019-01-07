/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { annotateFrames } from "../annotateFrames";

describe("annotateFrames", () => {
  it("should return Angular", () => {
    const callstack = [
      {
        source: {
          url: "https://cdnjs.cloudflare.com/ajax/libs/angular/angular.js"
        }
      },
      {
        source: {
          url: "/node_modules/zone/zone.js"
        }
      },
      {
        source: {
          url: "https://cdnjs.cloudflare.com/ajax/libs/angular/angular.js"
        }
      }
    ];
    const frames = annotateFrames(callstack);
    expect(frames).toEqual(
      callstack.map(f => ({ ...f, library: "Angular" })),
      "Angular (and zone.js) callstack is annotated as expected"
    );
  });
});
