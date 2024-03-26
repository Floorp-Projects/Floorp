/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Given a breakpoint location object, throws if the breakpoint look invalid
 */
export function validateBreakpointLocation({
  sourceUrl,
  sourceId,
  line,
  column,
}) {
  if (!sourceUrl && !sourceId) {
    throw new Error(
      `Breakpoints expect to have either a sourceUrl or a sourceId.`
    );
  }
  if (sourceUrl && typeof sourceUrl != "string") {
    throw new Error(
      `Breakpoints expect to have sourceUrl string, got ${typeof sourceUrl} instead.`
    );
  }
  // sourceId may be undefined for some sources keyed by URL
  if (sourceId && typeof sourceId != "string") {
    throw new Error(
      `Breakpoints expect to have sourceId string, got ${typeof sourceId} instead.`
    );
  }
  if (typeof line != "number") {
    throw new Error(
      `Breakpoints expect to have line number, got ${typeof line} instead.`
    );
  }
  if (typeof column != "number") {
    throw new Error(
      `Breakpoints expect to have column number, got ${typeof column} instead.`
    );
  }
}
