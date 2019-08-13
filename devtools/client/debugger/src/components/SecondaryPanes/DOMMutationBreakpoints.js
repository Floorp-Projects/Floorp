/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";

import Reps from "devtools-reps";
const {
  REPS: { Rep },
  MODE,
} = Reps;
import { translateNodeFrontToGrip } from "inspector-shared-utils";

import {
  deleteDOMMutationBreakpoint,
  toggleDOMMutationBreakpointState,
} from "framework-actions";

import actions from "../../actions";
import { connect } from "../../utils/connect";

import { CloseButton } from "../shared/Button";

import "./DOMMutationBreakpoints.css";
import type { DOMMutationBreakpoint } from "../../types";

type Props = {
  breakpoints: DOMMutationBreakpoint[],
  openElementInInspector: typeof actions.openElementInInspectorCommand,
  highlightDomElement: typeof actions.highlightDomElement,
  unHighlightDomElement: typeof actions.unHighlightDomElement,
  deleteBreakpoint: typeof deleteDOMMutationBreakpoint,
  toggleBreakpoint: typeof toggleDOMMutationBreakpointState,
};

const localizationTerms = {
  subtree: L10N.getStr("domMutationTypes.subtree"),
  attribute: L10N.getStr("domMutationTypes.attribute"),
  removal: L10N.getStr("domMutationTypes.removal"),
};

class DOMMutationBreakpointsContents extends Component<Props> {
  renderItem(breakpoint: DOMMutationBreakpoint) {
    const {
      openElementInInspector,
      highlightDomElement,
      unHighlightDomElement,
      toggleBreakpoint,
      deleteBreakpoint,
    } = this.props;
    return (
      <li>
        <input
          type="checkbox"
          checked={breakpoint.enabled}
          onChange={() => toggleBreakpoint(breakpoint.id, !breakpoint.enabled)}
        />
        <div className="dom-mutation-info">
          <div className="dom-mutation-label">
            {Rep({
              object: translateNodeFrontToGrip(breakpoint.nodeFront),
              mode: MODE.LONG,
              onDOMNodeClick: grip => openElementInInspector(grip),
              onInspectIconClick: grip => openElementInInspector(grip),
              onDOMNodeMouseOver: grip => highlightDomElement(grip),
              onDOMNodeMouseOut: grip => unHighlightDomElement(grip),
            })}
          </div>
          <div className="dom-mutation-type">
            {localizationTerms[breakpoint.mutationType] ||
              breakpoint.mutationType}
          </div>
        </div>
        <CloseButton
          handleClick={() =>
            deleteBreakpoint(breakpoint.nodeFront, breakpoint.mutationType)
          }
        />
      </li>
    );
  }

  renderEmpty() {
    return (
      <div className="dom-mutation-empty">
        {L10N.getStr("noDomMutationBreakpointsText")}
      </div>
    );
  }

  render() {
    const { breakpoints } = this.props;

    if (breakpoints.length === 0) {
      return this.renderEmpty();
    }

    return (
      <ul className="dom-mutation-list">
        {breakpoints.map(breakpoint => this.renderItem(breakpoint))}
      </ul>
    );
  }
}

const mapStateToProps = state => ({
  breakpoints: state.domMutationBreakpoints.breakpoints,
});

const DOMMutationBreakpointsPanel = connect(
  mapStateToProps,
  {
    deleteBreakpoint: deleteDOMMutationBreakpoint,
    toggleBreakpoint: toggleDOMMutationBreakpointState,
  },
  undefined,
  { storeKey: "toolbox-store" }
)(DOMMutationBreakpointsContents);

class DomMutationBreakpoints extends Component<Props> {
  render() {
    return (
      <DOMMutationBreakpointsPanel
        openElementInInspector={this.props.openElementInInspector}
        highlightDomElement={this.props.highlightDomElement}
        unHighlightDomElement={this.props.unHighlightDomElement}
      />
    );
  }
}

export default connect(
  undefined,
  {
    // the debugger-specific action bound to the debugger store
    // since there is no `storeKey`
    openElementInInspector: actions.openElementInInspectorCommand,
    highlightDomElement: actions.highlightDomElement,
    unHighlightDomElement: actions.unHighlightDomElement,
  }
)(DomMutationBreakpoints);
