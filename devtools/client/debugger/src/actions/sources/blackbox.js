/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Redux actions for the sources state
 * @module actions/sources
 */

import { originalToGeneratedId } from "devtools/client/shared/source-map-loader/index";
import { recordEvent } from "../../utils/telemetry";
import { toggleBreakpoints } from "../breakpoints/index";
import {
  getSourceActorsForSource,
  isSourceBlackBoxed,
  getBlackBoxRanges,
  getBreakpointsForSource,
} from "../../selectors/index";

export async function blackboxSourceActorsForSource(
  thunkArgs,
  source,
  shouldBlackBox,
  ranges = []
) {
  const { getState, client, sourceMapLoader } = thunkArgs;
  let sourceId = source.id;
  // If the source is the original, then get the source id of its generated file
  // and the range for where the original is represented in the generated file
  // (which might be a bundle including other files).
  if (source.isOriginal) {
    sourceId = originalToGeneratedId(source.id);
    const range = await sourceMapLoader.getFileGeneratedRange(source.id);
    ranges = [];
    if (range) {
      ranges.push(range);
      // TODO bug 1752108: Investigate blackboxing lines in original files,
      // there is likely to be issues as the whole genrated file
      // representing the original file will always be blackboxed.
      console.warn(
        "The might be unxpected issues when ignoring lines in an original file. " +
          "The whole original source is being blackboxed."
      );
    } else {
      throw new Error(
        `Unable to retrieve generated ranges for original source ${source.url}`
      );
    }
  }

  for (const actor of getSourceActorsForSource(getState(), sourceId)) {
    await client.blackBox(actor, shouldBlackBox, ranges);
  }
}

/**
 * Toggle blackboxing for the whole source or for specific lines in a source
 *
 * @param {Object} source - The source to be blackboxed/unblackboxed.
 * @param {Boolean} [shouldBlackBox] - Specifies if the source should be blackboxed (true
 *                                     or unblackboxed (false). When this is not provided
 *                                     option is decided based on the blackboxed state
 *                                     of the source.
 * @param {Array} [ranges] - List of line/column offsets to blackbox, these
 *                           are provided only when blackboxing lines.
 *                           The range structure:
 *                           const range = {
 *                            start: { line: 1, column: 5 },
 *                            end: { line: 3, column: 4 },
 *                           }
 */
export function toggleBlackBox(source, shouldBlackBox, ranges = []) {
  return async thunkArgs => {
    const { dispatch, getState } = thunkArgs;

    shouldBlackBox =
      typeof shouldBlackBox == "boolean"
        ? shouldBlackBox
        : !isSourceBlackBoxed(getState(), source);

    await blackboxSourceActorsForSource(
      thunkArgs,
      source,
      shouldBlackBox,
      ranges
    );

    if (shouldBlackBox) {
      recordEvent("blackbox");
      // If ranges is an empty array, it would mean we are blackboxing the whole
      // source. To do that lets reset the content to an empty array.
      if (!ranges.length) {
        dispatch({ type: "BLACKBOX_WHOLE_SOURCES", sources: [source] });
        await toggleBreakpointsInBlackboxedSources({
          thunkArgs,
          shouldDisable: true,
          sources: [source],
        });
      } else {
        const currentRanges = getBlackBoxRanges(getState())[source.url] || [];
        ranges = ranges.filter(newRange => {
          // To avoid adding duplicate ranges make sure
          // no range already exists with same start and end lines.
          const duplicate = currentRanges.findIndex(
            r =>
              r.start.line == newRange.start.line &&
              r.end.line == newRange.end.line
          );
          return duplicate == -1;
        });
        dispatch({ type: "BLACKBOX_SOURCE_RANGES", source, ranges });
        await toggleBreakpointsInRangesForBlackboxedSource({
          thunkArgs,
          shouldDisable: true,
          source,
          ranges,
        });
      }
    } else {
      // if there are no ranges to blackbox, then we are unblackboxing
      // the whole source
      // eslint-disable-next-line no-lonely-if
      if (!ranges.length) {
        dispatch({ type: "UNBLACKBOX_WHOLE_SOURCES", sources: [source] });
        toggleBreakpointsInBlackboxedSources({
          thunkArgs,
          shouldDisable: false,
          sources: [source],
        });
      } else {
        dispatch({ type: "UNBLACKBOX_SOURCE_RANGES", source, ranges });
        const blackboxRanges = getBlackBoxRanges(getState());
        if (!blackboxRanges[source.url].length) {
          dispatch({ type: "UNBLACKBOX_WHOLE_SOURCES", sources: [source] });
        }
        await toggleBreakpointsInRangesForBlackboxedSource({
          thunkArgs,
          shouldDisable: false,
          source,
          ranges,
        });
      }
    }
  };
}

async function toggleBreakpointsInRangesForBlackboxedSource({
  thunkArgs,
  shouldDisable,
  source,
  ranges,
}) {
  const { dispatch, getState } = thunkArgs;
  for (const range of ranges) {
    const breakpoints = getBreakpointsForSource(getState(), source, range);
    await dispatch(toggleBreakpoints(shouldDisable, breakpoints));
  }
}

async function toggleBreakpointsInBlackboxedSources({
  thunkArgs,
  shouldDisable,
  sources,
}) {
  const { dispatch, getState } = thunkArgs;
  for (const source of sources) {
    const breakpoints = getBreakpointsForSource(getState(), source);
    await dispatch(toggleBreakpoints(shouldDisable, breakpoints));
  }
}

/*
 * Blackboxes a group of sources together
 *
 * @param {Array} sourcesToBlackBox - The list of sources to blackbox
 * @param {Boolean} shouldBlackbox - Specifies if the sources should blackboxed (true)
 *                                   or unblackboxed (false).
 */
export function blackBoxSources(sourcesToBlackBox, shouldBlackBox) {
  return async thunkArgs => {
    const { dispatch, getState } = thunkArgs;

    const sources = sourcesToBlackBox.filter(
      source => isSourceBlackBoxed(getState(), source) !== shouldBlackBox
    );

    if (!sources.length) {
      return;
    }

    for (const source of sources) {
      await blackboxSourceActorsForSource(thunkArgs, source, shouldBlackBox);
    }

    if (shouldBlackBox) {
      recordEvent("blackbox");
    }

    dispatch({
      type: shouldBlackBox
        ? "BLACKBOX_WHOLE_SOURCES"
        : "UNBLACKBOX_WHOLE_SOURCES",
      sources,
    });
    await toggleBreakpointsInBlackboxedSources({
      thunkArgs,
      shouldDisable: shouldBlackBox,
      sources,
    });
  };
}
