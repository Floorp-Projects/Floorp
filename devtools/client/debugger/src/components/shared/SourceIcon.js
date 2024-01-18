/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "devtools/client/shared/vendor/react";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";

import { connect } from "devtools/client/shared/vendor/react-redux";

import AccessibleImage from "./AccessibleImage";

import { getSourceClassnames } from "../../utils/source";
import {
  getSymbols,
  isSourceBlackBoxed,
  hasPrettyTab,
} from "../../selectors/index";

class SourceIcon extends PureComponent {
  static get propTypes() {
    return {
      modifier: PropTypes.func.isRequired,
      location: PropTypes.object.isRequired,
      iconClass: PropTypes.string,
      forTab: PropTypes.bool,
    };
  }

  render() {
    const { modifier } = this.props;
    let { iconClass } = this.props;

    if (modifier) {
      const modified = modifier(iconClass);
      if (!modified) {
        return null;
      }
      iconClass = modified;
    }
    return React.createElement(AccessibleImage, {
      className: `source-icon ${iconClass}`,
    });
  }
}

export default connect((state, props) => {
  const { forTab, location } = props;
  // BreakpointHeading sometimes spawn locations without source actor for generated sources
  // which disallows fetching symbols. In such race condition return the default icon.
  // (this reproduces when running browser_dbg-breakpoints-popup.js)
  if (!location.source.isOriginal && !location.sourceActor) {
    return "file";
  }
  const symbols = getSymbols(state, location);
  const isBlackBoxed = isSourceBlackBoxed(state, location.source);
  // For the tab icon, we don't want to show the pretty icon for the non-pretty tab
  const hasMatchingPrettyTab = !forTab && hasPrettyTab(state, location.source);

  // This is the key function that will compute the icon type,
  // In addition to the "modifier" implemented by each callsite.
  const iconClass = getSourceClassnames(
    location.source,
    symbols,
    isBlackBoxed,
    hasMatchingPrettyTab
  );

  return {
    iconClass,
  };
})(SourceIcon);
