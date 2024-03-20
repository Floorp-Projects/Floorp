/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import React, { Component } from "devtools/client/shared/vendor/react";
import { div } from "devtools/client/shared/vendor/react-dom-factories";
import Breakpoint from "./Breakpoint";

import {
  getSelectedSource,
  getFirstVisibleBreakpoints,
} from "../../selectors/index";
import { makeBreakpointId } from "../../utils/breakpoint/index";
import { connect } from "devtools/client/shared/vendor/react-redux";
import actions from "../../actions/index";

class Breakpoints extends Component {
  static get propTypes() {
    return {
      breakpoints: PropTypes.array,
      editor: PropTypes.object,
      selectedSource: PropTypes.object,
      removeBreakpointsAtLine: PropTypes.func,
      toggleBreakpointsAtLine: PropTypes.func,
      continueToHere: PropTypes.func,
      showEditorEditBreakpointContextMenu: PropTypes.func,
    };
  }
  render() {
    const {
      breakpoints,
      selectedSource,
      editor,
      showEditorEditBreakpointContextMenu,
      continueToHere,
      toggleBreakpointsAtLine,
      removeBreakpointsAtLine,
    } = this.props;

    if (!selectedSource || !breakpoints) {
      return null;
    }
    return div(
      null,
      breakpoints.map(breakpoint => {
        return React.createElement(Breakpoint, {
          key: makeBreakpointId(breakpoint.location),
          breakpoint,
          selectedSource,
          showEditorEditBreakpointContextMenu,
          continueToHere,
          toggleBreakpointsAtLine,
          removeBreakpointsAtLine,
          editor,
        });
      })
    );
  }
}

const mapStateToProps = state => {
  const selectedSource = getSelectedSource(state);
  return {
    // Retrieves only the first breakpoint per line so that the
    // breakpoint marker represents only the first breakpoint
    breakpoints: getFirstVisibleBreakpoints(state),
    selectedSource,
  };
};

export default connect(mapStateToProps, {
  showEditorEditBreakpointContextMenu:
    actions.showEditorEditBreakpointContextMenu,
  continueToHere: actions.continueToHere,
  toggleBreakpointsAtLine: actions.toggleBreakpointsAtLine,
  removeBreakpointsAtLine: actions.removeBreakpointsAtLine,
})(Breakpoints);
