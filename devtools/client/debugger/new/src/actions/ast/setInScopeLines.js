/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getOutOfScopeLocations, getSelectedSource } from "../../selectors";
import { getSourceLineCount } from "../../utils/source";

import { range, flatMap, uniq, without } from "lodash";

function getOutOfScopeLines(outOfScopeLocations: AstLocation[]) {
  if (!outOfScopeLocations) {
    return null;
  }

  return uniq(
    flatMap(outOfScopeLocations, location =>
      range(location.start.line, location.end.line)
    )
  );
}

export function setInScopeLines() {
  return ({ dispatch, getState }: ThunkArgs) => {
    const source = getSelectedSource(getState());
    const outOfScopeLocations = getOutOfScopeLocations(getState());

    if (!source || !source.text) {
      return;
    }

    const linesOutOfScope = getOutOfScopeLines(outOfScopeLocations);

    const sourceNumLines = getSourceLineCount(source);
    const sourceLines = range(1, sourceNumLines + 1);

    const inScopeLines = !linesOutOfScope
      ? sourceLines
      : without(sourceLines, ...linesOutOfScope);

    dispatch({
      type: "IN_SCOPE_LINES",
      lines: inScopeLines
    });
  };
}
