/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSelector } from "reselect";

import {
  getViewport,
  getSource,
  getSelectedSource,
  getSelectedSourceTextContent,
  getBreakpointPositions,
  getBreakpointPositionsForSource,
} from "./index";
import { getVisibleBreakpoints } from "./visibleBreakpoints";
import { getSelectedLocation } from "../utils/selected-location";
import { sortSelectedLocations } from "../utils/location";
import { getLineText } from "../utils/source";

function contains(location, range) {
  return (
    location.line >= range.start.line &&
    location.line <= range.end.line &&
    (!location.column ||
      (location.column >= range.start.column &&
        location.column <= range.end.column))
  );
}

function groupBreakpoints(breakpoints, selectedSource) {
  const breakpointsMap = {};
  if (!breakpoints) {
    return breakpointsMap;
  }

  for (const breakpoint of breakpoints) {
    if (breakpoint.options.hidden) {
      continue;
    }
    const location = getSelectedLocation(breakpoint, selectedSource);
    const { line, column } = location;

    if (!breakpointsMap[line]) {
      breakpointsMap[line] = {};
    }

    if (!breakpointsMap[line][column]) {
      breakpointsMap[line][column] = [];
    }

    breakpointsMap[line][column].push(breakpoint);
  }

  return breakpointsMap;
}

function findBreakpoint(location, breakpointMap) {
  const { line, column } = location;
  const breakpoints = breakpointMap[line]?.[column];

  if (!breakpoints) {
    return null;
  }
  return breakpoints[0];
}

function filterByLineCount(positions, selectedSource) {
  const lineCount = {};

  for (const breakpoint of positions) {
    const { line } = getSelectedLocation(breakpoint, selectedSource);
    if (!lineCount[line]) {
      lineCount[line] = 0;
    }
    lineCount[line] = lineCount[line] + 1;
  }

  return positions.filter(
    breakpoint =>
      lineCount[getSelectedLocation(breakpoint, selectedSource).line] > 1
  );
}

function filterVisible(positions, selectedSource, viewport) {
  return positions.filter(columnBreakpoint => {
    const location = getSelectedLocation(columnBreakpoint, selectedSource);
    return viewport && contains(location, viewport);
  });
}

function filterByBreakpoints(positions, selectedSource, breakpointMap) {
  return positions.filter(position => {
    const location = getSelectedLocation(position, selectedSource);
    return breakpointMap[location.line];
  });
}

// Filters out breakpoints to the right of the line. (bug 1552039)
function filterInLine(positions, selectedSource, selectedContent) {
  return positions.filter(position => {
    const location = getSelectedLocation(position, selectedSource);
    const lineText = getLineText(
      selectedSource.id,
      selectedContent,
      location.line
    );

    return lineText.length >= (location.column || 0);
  });
}

function formatPositions(positions, selectedSource, breakpointMap) {
  return positions.map(position => {
    const location = getSelectedLocation(position, selectedSource);
    return {
      location,
      breakpoint: findBreakpoint(location, breakpointMap),
    };
  });
}

function convertToList(breakpointPositions) {
  return [].concat(...Object.values(breakpointPositions));
}

export function getColumnBreakpoints(
  positions,
  breakpoints,
  viewport,
  selectedSource,
  selectedSourceTextContent
) {
  if (!positions || !selectedSource) {
    return [];
  }

  // We only want to show a column breakpoint if several conditions are matched
  // - it is the first breakpoint to appear at an the original location
  // - the position is in the current viewport
  // - there is atleast one other breakpoint on that line
  // - there is a breakpoint on that line
  const breakpointMap = groupBreakpoints(breakpoints, selectedSource);
  positions = filterByLineCount(positions, selectedSource);
  positions = filterVisible(positions, selectedSource, viewport);
  positions = filterInLine(
    positions,
    selectedSource,
    selectedSourceTextContent
  );
  positions = filterByBreakpoints(positions, selectedSource, breakpointMap);

  return formatPositions(positions, selectedSource, breakpointMap);
}

const getVisibleBreakpointPositions = createSelector(
  getSelectedSource,
  getBreakpointPositions,
  (source, positions) => {
    if (!source) {
      return [];
    }

    const sourcePositions = positions[source.id];
    if (!sourcePositions) {
      return [];
    }

    return convertToList(sourcePositions);
  }
);

export const visibleColumnBreakpoints = createSelector(
  getVisibleBreakpointPositions,
  getVisibleBreakpoints,
  getViewport,
  getSelectedSource,
  getSelectedSourceTextContent,
  getColumnBreakpoints
);

export function getFirstBreakpointPosition(state, { line, sourceId }) {
  const positions = getBreakpointPositionsForSource(state, sourceId);
  const source = getSource(state, sourceId);

  if (!source || !positions) {
    return null;
  }

  return sortSelectedLocations(convertToList(positions), source).find(
    position => getSelectedLocation(position, source).line == line
  );
}
