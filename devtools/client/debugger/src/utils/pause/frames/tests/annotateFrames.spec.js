/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { annotateFrames } from "../annotateFrames";
import { makeMockFrameWithURL } from "../../../test-mockup";

describe("annotateFrames", () => {
  it("should return Angular", () => {
    const callstack = [
      makeMockFrameWithURL(
        "https://stackblitz.io/turbo_modules/@angular/core@7.2.4/bundles/core.umd.js"
      ),
      makeMockFrameWithURL("/node_modules/zone/zone.js"),
      makeMockFrameWithURL(
        "https://cdnjs.cloudflare.com/ajax/libs/angular/angular.js"
      ),
    ];
    const frames = annotateFrames(callstack);
    expect(frames).toEqual(callstack.map(f => ({ ...f, library: "Angular" })));
  });
});
