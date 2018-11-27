/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import { connect } from "react-redux";

import { range, keyBy, isEqualWith, uniqBy, groupBy, flatten } from "lodash";

import CallSite from "./CallSite";

import {
  getSelectedSource,
  getSymbols,
  getSelectedLocation,
  getBreakpointsForSource
} from "../../selectors";

import { getTokenLocation } from "../../utils/editor";
import { isWasm } from "../../utils/wasm";

import actions from "../../actions";

function getCallSiteAtLocation(callSites, location) {
  return callSites.find(callSite =>
    isEqualWith(callSite.location, location, (cloc, loc) => {
      return (
        loc.line === cloc.start.line &&
        (loc.column >= cloc.start.column && loc.column <= cloc.end.column)
      );
    })
  );
}

class CallSites extends Component {
  props: {
    symbols: Array<Symbol>,
    callSites: Array<Symbol>,
    editor: Object,
    breakpoints: Map,
    addBreakpoint: Function,
    removeBreakpoint: Function,
    selectedSource: Object,
    selectedLocation: Object
  };

  componentDidMount() {
    const { editor } = this.props;
    const codeMirrorWrapper = editor.codeMirror.getWrapperElement();

    codeMirrorWrapper.addEventListener("click", e => this.onTokenClick(e));
  }

  componentWillUnmount() {
    const { editor } = this.props;
    const codeMirrorWrapper = editor.codeMirror.getWrapperElement();

    codeMirrorWrapper.removeEventListener("click", e => this.onTokenClick(e));
  }

  onTokenClick(e) {
    const { target } = e;
    const { editor, selectedLocation } = this.props;

    if (
      !target.classList.contains("call-site") &&
      !target.classList.contains("call-site-bp")
    ) {
      return;
    }

    const { sourceId } = selectedLocation;
    const { line, column } = getTokenLocation(editor.codeMirror, target);

    this.toggleBreakpoint(line, isWasm(sourceId) ? undefined : column);
  }

  toggleBreakpoint(line, column = undefined) {
    const {
      selectedSource,
      selectedLocation,
      addBreakpoint,
      removeBreakpoint,
      callSites
    } = this.props;

    const callSite = getCallSiteAtLocation(callSites, { line, column });

    if (!callSite) {
      return;
    }

    const bp = callSite.breakpoint;

    if ((bp && bp.loading) || !selectedLocation || !selectedSource) {
      return;
    }

    const { sourceId } = selectedLocation;

    if (bp) {
      // NOTE: it's possible the breakpoint has slid to a column
      column = column || bp.location.column;
      removeBreakpoint({
        sourceId: sourceId,
        line: line,
        column
      });
    } else {
      addBreakpoint({
        sourceId: sourceId,
        sourceUrl: selectedSource.url,
        line: line,
        column: column
      });
    }
  }

  // Return the call sites that are on the same line as an
  // existing line breakpoint
  filterCallSitesByLineNumber() {
    const { callSites, breakpoints } = this.props;

    // Get unique lines from breakpoints so we can filter out unwated call sites
    const uniqueBreakpointLines = new Set(
      breakpoints.map(bp => bp.location.line)
    );

    // Get call sites based on activated breakpoint lines
    const callSitesInRange = callSites.filter(({ location }) =>
      uniqueBreakpointLines.has(location.start.line)
    );

    // Group call sites by line
    const callSitesByLineObj = groupBy(callSitesInRange, "location.start.line");

    // Per group, ensure all call sites are unique
    return flatten(
      Object.values(callSitesByLineObj).map(arr => {
        const uniques = uniqBy(
          arr,
          site =>
            `${site.generatedLocation.line}:${site.generatedLocation.column}`
        );
        // Only return call sites for a line when more than 1 is found
        return uniques.length > 1 ? uniques : [];
      })
    );
  }

  render() {
    const { editor, callSites, selectedSource, breakpoints } = this.props;

    if (!callSites || breakpoints.length === 0) {
      return null;
    }

    const callSitesFiltered = this.filterCallSitesByLineNumber();

    let sites;
    editor.codeMirror.operation(() => {
      const childCallSites = callSitesFiltered.map((callSite, index) => {
        const props = {
          key: index,
          callSite,
          editor,
          source: selectedSource,
          breakpoint: callSite.breakpoint,
          showCallSite: true
        };
        return <CallSite {...props} />;
      });
      sites = <div>{childCallSites}</div>;
    });
    return sites;
  }
}

function getCallSites(symbols, breakpoints) {
  if (!symbols || !symbols.callExpressions) {
    return;
  }

  const callSites = symbols.callExpressions;

  // NOTE: we create a breakpoint map keyed on location
  // to speed up the lookups. Hopefully we'll fix the
  // inconsistency with column offsets so that we can expect
  // a breakpoint to be added at the beginning of a call expression.
  const bpLocationMap = keyBy(breakpoints, ({ location }) =>
    locationKey(location)
  );

  function locationKey({ line, column }) {
    return `${line}/${column}`;
  }

  function findBreakpoint(callSite) {
    const {
      location: { start, end }
    } = callSite;

    const breakpointId = range(start.column - 1, end.column)
      .map(column => locationKey({ line: start.line, column }))
      .find(key => bpLocationMap[key]);

    if (breakpointId) {
      return bpLocationMap[breakpointId];
    }
  }

  return callSites
    .filter(({ location }) => location.start.line === location.end.line)
    .map(callSite => ({ ...callSite, breakpoint: findBreakpoint(callSite) }));
}

const mapStateToProps = state => {
  const selectedLocation = getSelectedLocation(state);
  const selectedSource = getSelectedSource(state);
  const sourceId = selectedLocation && selectedLocation.sourceId;
  const symbols = getSymbols(state, selectedSource);
  const breakpoints = getBreakpointsForSource(state, sourceId);

  return {
    selectedLocation,
    selectedSource,
    callSites: getCallSites(symbols, breakpoints),
    breakpoints: breakpoints
  };
};

const { addBreakpoint, removeBreakpoint } = actions;
const mapDispatchToProps = { addBreakpoint, removeBreakpoint };

export default connect(
  mapStateToProps,
  mapDispatchToProps
)(CallSites);
