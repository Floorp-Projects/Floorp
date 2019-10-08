/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { PureComponent } from "react";

import { connect } from "../../utils/connect";

import AccessibleImage from "./AccessibleImage";

import { getSourceClassnames } from "../../utils/source";
import { getFramework } from "../../utils/tabs";
import { getSymbols, getTabs } from "../../selectors";

import type { Source } from "../../types";
import type { Symbols } from "../../reducers/types";

import "./SourceIcon.css";

type OwnProps = {|
  source: Source,

  // An additional validator for the icon returned
  shouldHide?: string => boolean,
|};
type Props = {
  source: Source,
  shouldHide?: string => boolean,

  // symbols will provide framework information
  symbols: ?Symbols,
  framework: ?string,
};

class SourceIcon extends PureComponent<Props> {
  render() {
    const { shouldHide, source, symbols, framework } = this.props;
    const iconClass = framework
      ? framework.toLowerCase()
      : getSourceClassnames(source, symbols);

    if (shouldHide && shouldHide(iconClass)) {
      return null;
    }

    return <AccessibleImage className={`source-icon ${iconClass}`} />;
  }
}

export default connect<Props, OwnProps, _, _, _, _>((state, props) => ({
  symbols: getSymbols(state, props.source),
  framework: getFramework(getTabs(state), props.source.url),
}))(SourceIcon);
