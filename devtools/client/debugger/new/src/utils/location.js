/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { SourceLocation, SourceId } from "../types";

type IncompleteLocation = {
  sourceId: SourceId,
  line?: number,
  column?: number,
  sourceUrl?: string
};

export function createLocation({
  sourceId,
  line,
  column,
  sourceUrl
}: IncompleteLocation): SourceLocation {
  return {
    sourceId,
    line: line || 0,
    column,
    sourceUrl: sourceUrl || ""
  };
}
