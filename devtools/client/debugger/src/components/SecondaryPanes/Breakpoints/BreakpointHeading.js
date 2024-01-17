/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "devtools/client/shared/vendor/react";
import { div, span } from "devtools/client/shared/vendor/react-dom-factories";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";

import { connect } from "devtools/client/shared/vendor/react-redux";
import actions from "../../../actions/index";

import {
  getTruncatedFileName,
  getDisplayPath,
  getSourceQueryString,
  getFileURL,
} from "../../../utils/source";
import { createLocation } from "../../../utils/location";
import { getFirstSourceActorForGeneratedSource } from "../../../selectors/index";

import SourceIcon from "../../shared/SourceIcon";

class BreakpointHeading extends PureComponent {
  static get propTypes() {
    return {
      sources: PropTypes.array.isRequired,
      source: PropTypes.object.isRequired,
      firstSourceActor: PropTypes.object,
      selectSource: PropTypes.func.isRequired,
      showBreakpointHeadingContextMenu: PropTypes.func.isRequired,
    };
  }
  onContextMenu = event => {
    event.preventDefault();

    this.props.showBreakpointHeadingContextMenu(event, this.props.source);
  };

  render() {
    const { sources, source, selectSource } = this.props;

    const path = getDisplayPath(source, sources);
    const query = getSourceQueryString(source);
    return div(
      {
        className: "breakpoint-heading",
        title: getFileURL(source, false),
        onClick: () => selectSource(source),
        onContextMenu: this.onContextMenu,
      },
      React.createElement(
        SourceIcon,
        // Breakpoints are displayed per source and may relate to many source actors.
        // Arbitrarily pick the first source actor to compute the matching source icon
        // The source actor is used to pick one specific source text content and guess
        // the related framework icon.
        {
          location: createLocation({
            source,
            sourceActor: this.props.firstSourceActor,
          }),
          modifier: icon =>
            ["file", "javascript"].includes(icon) ? null : icon,
        }
      ),
      div(
        {
          className: "filename",
        },
        getTruncatedFileName(source, query),
        path && span(null, `../${path}/..`)
      )
    );
  }
}

const mapStateToProps = (state, { source }) => ({
  firstSourceActor: getFirstSourceActorForGeneratedSource(state, source.id),
});

export default connect(mapStateToProps, {
  selectSource: actions.selectSource,
  showBreakpointHeadingContextMenu: actions.showBreakpointHeadingContextMenu,
})(BreakpointHeading);
