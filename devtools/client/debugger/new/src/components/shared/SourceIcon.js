/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { PureComponent } from "react";

import { connect } from "react-redux";

import AccessibleImage from "./AccessibleImage";

import { getSourceClassnames } from "../../utils/source";
import { getFramework } from "../../utils/tabs";
import { getSourceMetaData, getTabs } from "../../selectors";

import type { Source } from "../../types";
import type { SourceMetaDataType } from "../../reducers/ast";

import "./SourceIcon.css";

type Props = {
  source: Source,
  // sourceMetaData will provide framework information
  sourceMetaData: SourceMetaDataType,
  // An additional validator for the icon returned
  shouldHide?: Function,
  framework?: string
};

class SourceIcon extends PureComponent<Props> {
  render() {
    const { shouldHide, source, sourceMetaData, framework } = this.props;
    const iconClass = framework
      ? framework.toLowerCase()
      : getSourceClassnames(source, sourceMetaData);

    if (shouldHide && shouldHide(iconClass)) {
      return null;
    }

    return <AccessibleImage className={`source-icon ${iconClass}`} />;
  }
}

export default connect((state, props) => {
  return {
    sourceMetaData: getSourceMetaData(state, props.source.id),
    framework: getFramework(getTabs(state), props.source.url)
  };
})(SourceIcon);
