/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";

import Reps from "devtools/client/shared/components/reps/index";
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

const localizationTerms = {
  subtree: L10N.getStr("domMutationTypes.subtree"),
  attribute: L10N.getStr("domMutationTypes.attribute"),
  removal: L10N.getStr("domMutationTypes.removal"),
};

class DOMMutationBreakpointsContents extends Component {
  handleBreakpoint(breakpointId, shouldEnable) {
    const { toggleBreakpoint, setSkipPausing } = this.props;

    // The user has enabled a mutation breakpoint so we should no
    // longer skip pausing
    if (shouldEnable) {
      setSkipPausing(false);
    }
    toggleBreakpoint(breakpointId, shouldEnable);
  }

  renderItem(breakpoint) {
    const {
      openElementInInspector,
      highlightDomElement,
      unHighlightDomElement,
      deleteBreakpoint,
    } = this.props;
    const { enabled, id: breakpointId, nodeFront, mutationType } = breakpoint;

    return (
      <li key={breakpoint.id}>
        <input
          type="checkbox"
          checked={enabled}
          onChange={() => this.handleBreakpoint(breakpointId, !enabled)}
        />
        <div className="dom-mutation-info">
          <div className="dom-mutation-label">
            {Rep({
              object: translateNodeFrontToGrip(nodeFront),
              mode: MODE.TINY,
              onDOMNodeClick: () => openElementInInspector(nodeFront),
              onInspectIconClick: () => openElementInInspector(nodeFront),
              onDOMNodeMouseOver: () => highlightDomElement(nodeFront),
              onDOMNodeMouseOut: () => unHighlightDomElement(),
            })}
          </div>
          <div className="dom-mutation-type">
            {localizationTerms[mutationType] || mutationType}
          </div>
        </div>
        <CloseButton
          handleClick={() => deleteBreakpoint(nodeFront, mutationType)}
        />
      </li>
    );
  }

  /* eslint-disable react/no-danger */
  renderEmpty() {
    const { openInspector } = this.props;
    const text = L10N.getFormatStr(
      "noDomMutationBreakpoints",
      `<a>${L10N.getStr("inspectorTool")}</a>`
    );

    return (
      <div className="dom-mutation-empty">
        <div
          onClick={() => openInspector()}
          dangerouslySetInnerHTML={{ __html: text }}
        />
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

class DomMutationBreakpoints extends Component {
  render() {
    return (
      <DOMMutationBreakpointsPanel
        openElementInInspector={this.props.openElementInInspector}
        highlightDomElement={this.props.highlightDomElement}
        unHighlightDomElement={this.props.unHighlightDomElement}
        setSkipPausing={this.props.setSkipPausing}
        openInspector={this.props.openInspector}
      />
    );
  }
}

export default connect(undefined, {
  // the debugger-specific action bound to the debugger store
  // since there is no `storeKey`
  openElementInInspector: actions.openElementInInspectorCommand,
  highlightDomElement: actions.highlightDomElement,
  unHighlightDomElement: actions.unHighlightDomElement,
  setSkipPausing: actions.setSkipPausing,
  openInspector: actions.openInspector,
})(DomMutationBreakpoints);
