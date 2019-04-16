/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import typeof SourceMaps from "devtools-source-map";

import type { Breakpoint } from "../../types";

export default function remapLocations(
  breakpoints: Breakpoint[],
  sourceId: string,
  sourceMaps: SourceMaps
) {
  const sourceBreakpoints: Promise<Breakpoint>[] = breakpoints.map(
    async breakpoint => {
      if (breakpoint.location.sourceId !== sourceId) {
        return breakpoint;
      }
      const location = await sourceMaps.getOriginalLocation(
        breakpoint.location
      );
      return { ...breakpoint, location };
    }
  );

  return Promise.all(sourceBreakpoints);
}
