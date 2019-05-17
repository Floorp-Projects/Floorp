/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  getOutOfScopeLocations,
  getSelectedSourceWithContent,
} from "../../selectors";
import { getSourceLineCount } from "../../utils/source";

import { range, flatMap, uniq, without } from "lodash";
import { isFulfilled } from "../../utils/async-value";

import type { AstLocation } from "../../workers/parser";
import type { ThunkArgs } from "../types";
import type { Context } from "../../types";

function getOutOfScopeLines(outOfScopeLocations: ?(AstLocation[])) {
  if (!outOfScopeLocations) {
    return null;
  }

  return uniq(
    flatMap(outOfScopeLocations, location =>
      range(location.start.line, location.end.line)
    )
  );
}

export function setInScopeLines(cx: Context) {
  return ({ dispatch, getState }: ThunkArgs) => {
    const sourceWithContent = getSelectedSourceWithContent(getState());
    const outOfScopeLocations = getOutOfScopeLocations(getState());

    if (!sourceWithContent || !sourceWithContent.content) {
      return;
    }
    const content = sourceWithContent.content;

    const linesOutOfScope = getOutOfScopeLines(outOfScopeLocations);

    const sourceNumLines = isFulfilled(content)
      ? getSourceLineCount(content.value)
      : 0;
    const sourceLines = range(1, sourceNumLines + 1);

    const inScopeLines = !linesOutOfScope
      ? sourceLines
      : without(sourceLines, ...linesOutOfScope);

    dispatch({
      type: "IN_SCOPE_LINES",
      cx,
      lines: inScopeLines,
    });
  };
}
