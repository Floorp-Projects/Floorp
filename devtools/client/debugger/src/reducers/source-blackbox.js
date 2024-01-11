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

    blackboxedSet: state?.blackboxedRanges
      ? new Set(Object.keys(state.blackboxedRanges))
      : new Set(),

    sourceMapIgnoreListUrls: [],
  };
}

function update(state = initialSourceBlackBoxState(), action) {
  switch (action.type) {
    case "BLACKBOX_WHOLE_SOURCES": {
      const { sources } = action;

      const currentBlackboxedRanges = { ...state.blackboxedRanges };
      const currentBlackboxedSet = new Set(state.blackboxedSet);

      for (const source of sources) {
        currentBlackboxedRanges[source.url] = [];
        currentBlackboxedSet.add(source.url);
      }

      return {
        ...state,
        blackboxedRanges: currentBlackboxedRanges,
        blackboxedSet: currentBlackboxedSet,
      };
    }

    case "BLACKBOX_SOURCE_RANGES": {
      const { source, ranges } = action;

      const currentBlackboxedRanges = { ...state.blackboxedRanges };
      const currentBlackboxedSet = new Set(state.blackboxedSet);

      if (!currentBlackboxedRanges[source.url]) {
        currentBlackboxedRanges[source.url] = [];
        currentBlackboxedSet.add(source.url);
      } else {
        currentBlackboxedRanges[source.url] = [
          ...state.blackboxedRanges[source.url],
        ];
      }

      // Add new blackboxed lines in acsending order
      for (const newRange of ranges) {
        const index = currentBlackboxedRanges[source.url].findIndex(
          range =>
            range.end.line <= newRange.start.line &&
            range.end.column <= newRange.start.column
        );
        currentBlackboxedRanges[source.url].splice(index + 1, 0, newRange);
      }

      return {
        ...state,
        blackboxedRanges: currentBlackboxedRanges,
        blackboxedSet: currentBlackboxedSet,
      };
    }

    case "UNBLACKBOX_WHOLE_SOURCES": {
      const { sources } = action;

      const currentBlackboxedRanges = { ...state.blackboxedRanges };
      const currentBlackboxedSet = new Set(state.blackboxedSet);

      for (const source of sources) {
        delete currentBlackboxedRanges[source.url];
        currentBlackboxedSet.delete(source.url);
      }

      return {
        ...state,
        blackboxedRanges: currentBlackboxedRanges,
        blackboxedSet: currentBlackboxedSet,
      };
    }

    case "UNBLACKBOX_SOURCE_RANGES": {
      const { source, ranges } = action;

      const currentBlackboxedRanges = {
        ...state.blackboxedRanges,
        [source.url]: [...state.blackboxedRanges[source.url]],
      };

      for (const newRange of ranges) {
        const index = currentBlackboxedRanges[source.url].findIndex(
          range =>
            range.start.line === newRange.start.line &&
            range.end.line === newRange.end.line
        );

        if (index !== -1) {
          currentBlackboxedRanges[source.url].splice(index, 1);
        }
      }

      return {
        ...state,
        blackboxedRanges: currentBlackboxedRanges,
      };
    }

    case "ADD_SOURCEMAP_IGNORE_LIST_SOURCES": {
      return {
        ...state,
        sourceMapIgnoreListUrls: [
          ...state.sourceMapIgnoreListUrls,
          ...action.ignoreListUrls,
        ],
      };
    }

    case "NAVIGATE":
      return initialSourceBlackBoxState(state);
  }

  return state;
}

export default update;
