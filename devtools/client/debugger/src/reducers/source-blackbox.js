/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Reducer containing all data about sources being "black boxed".
 *
 * i.e. sources which should be ignored by the debugger.
 * Typically, these sources should be hidden from paused stack frames,
 * and any debugger statement or breakpoint should be ignored.
 */

export function initialSourceBlackBoxState(state) {
  return {
    /* FORMAT:
     * blackboxedRanges: {
     *  [source url]: [range, range, ...], -- source lines blackboxed
     *  [source url]: [], -- whole source blackboxed
     *  ...
     * }
     */
    blackboxedRanges: state?.blackboxedRanges ?? {},

    blackboxedSet: new Set(),
  };
}

function update(state = initialSourceBlackBoxState(), action) {
  switch (action.type) {
    case "BLACKBOX":
      if (action.status === "done") {
        const { blackboxSources } = action.value;
        state = updateBlackBoxState(state, blackboxSources);
      }
      break;

    case "NAVIGATE":
      return initialSourceBlackBoxState(state);
  }

  return state;
}

function updateBlackboxRangesForSourceUrl(
  currentRanges,
  currentSet,
  url,
  shouldBlackBox,
  newRanges
) {
  if (shouldBlackBox) {
    currentSet.add(url);
    // If newRanges is an empty array, it would mean we are blackboxing the whole
    // source. To do that lets reset the content to an empty array.
    if (!newRanges.length) {
      currentRanges[url] = [];
    } else {
      currentRanges[url] = currentRanges[url] || [];
      newRanges.forEach(newRange => {
        // To avoid adding duplicate ranges make sure
        // no range alredy exists with same start and end lines.
        const duplicate = currentRanges[url].findIndex(
          r =>
            r.start.line == newRange.start.line &&
            r.end.line == newRange.end.line
        );
        if (duplicate !== -1) {
          return;
        }
        // ranges are sorted in asc
        const index = currentRanges[url].findIndex(
          range =>
            range.end.line <= newRange.start.line &&
            range.end.column <= newRange.start.column
        );
        currentRanges[url].splice(index + 1, 0, newRange);
      });
    }
  } else {
    // if there are no ranges to blackbox, then we are unblackboxing
    // the whole source
    if (!newRanges.length) {
      currentSet.delete(url);
      delete currentRanges[url];
      return;
    }
    // Remove only the lines represented by the ranges provided.
    newRanges.forEach(newRange => {
      const index = currentRanges[url].findIndex(
        range =>
          range.start.line === newRange.start.line &&
          range.end.line === newRange.end.line
      );

      if (index !== -1) {
        currentRanges[url].splice(index, 1);
      }
    });

    // if the last blackboxed line has been removed, unblackbox the source.
    if (!currentRanges[url].length) {
      currentSet.delete(url);
      delete currentRanges[url];
    }
  }
}

/*
 * Updates the all the state necessary for blackboxing
 *
 */
function updateBlackBoxState(state, blackboxSources) {
  const currentRanges = { ...state.blackboxedRanges };
  const currentSet = new Set(state.blackboxedSet);
  blackboxSources.map(({ source, shouldBlackBox, ranges }) =>
    updateBlackboxRangesForSourceUrl(
      currentRanges,
      currentSet,
      source.url,
      shouldBlackBox,
      ranges
    )
  );
  return {
    ...state,
    blackboxedRanges: currentRanges,
    blackboxedSet: currentSet,
  };
}

export default update;
