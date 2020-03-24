/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { PureComponent } from "react";

import { connect } from "../../utils/connect";

import AccessibleImage from "./AccessibleImage";

import { getSourceClassnames, isPretty } from "../../utils/source";
import { getFramework } from "../../utils/tabs";
import { getSymbols, getTabs } from "../../selectors";

import type { Source } from "../../types";
import type { Symbols } from "../../reducers/types";

import "./SourceIcon.css";

type OwnProps = {|
  source: Source,

  // An additional validator for the icon returned
  modifier?: string => string | null,
|};
type Props = {
  source: Source,
  modifier?: string => string | null,

  // symbols will provide framework information
  symbols: ?Symbols,
  framework: ?string,
};

class SourceIcon extends PureComponent<Props> {
  render() {
    const { modifier, source, symbols, framework } = this.props;
    let iconClass = "";

    if (isPretty(source)) {
      iconClass = "prettyPrint";
    } else {
      iconClass = framework
        ? framework.toLowerCase()
        : getSourceClassnames(source, symbols);
    }

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

export default connect<Props, OwnProps, _, _, _, _>((state, props) => ({
  symbols: getSymbols(state, props.source),
  framework: getFramework(getTabs(state), props.source.url),
}))(SourceIcon);
