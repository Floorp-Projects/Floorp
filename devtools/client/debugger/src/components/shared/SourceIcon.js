/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "react";
import PropTypes from "prop-types";

import { connect } from "../../utils/connect";

import AccessibleImage from "./AccessibleImage";

import { getSourceClassnames } from "../../utils/source";
import { getSymbols, isSourceBlackBoxed, hasPrettyTab } from "../../selectors";

import "./SourceIcon.css";

class SourceIcon extends PureComponent {
  static get propTypes() {
    return {
      modifier: PropTypes.func.isRequired,
      source: PropTypes.object.isRequired,
      iconClass: PropTypes.string,
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

    return <AccessibleImage className={`source-icon ${iconClass}`} />;
  }
}

export default connect((state, props) => {
  const { source } = props;
  const symbols = getSymbols(state, source);
  const isBlackBoxed = isSourceBlackBoxed(state, source);
  const hasMatchingPrettyTab = hasPrettyTab(state, source.url);

  // This is the key function that will compute the icon type,
  // In addition to the "modifier" implemented by each callsite.
  const iconClass = getSourceClassnames(
    source,
    symbols,
    isBlackBoxed,
    hasMatchingPrettyTab
  );

  return {
    iconClass,
  };
})(SourceIcon);
