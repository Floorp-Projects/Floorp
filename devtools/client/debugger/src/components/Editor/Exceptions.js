/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { Component } from "react";
import { connect } from "../../utils/connect";

import Exception from "./Exception";

import {
  getSelectedSource,
  getSelectedSourceExceptions,
} from "../../selectors";
import { getDocument } from "../../utils/editor";

import type { Source, Exception as Exc } from "../../types";

type Props = {
  selectedSource: ?Source,
  exceptions: Exc[],
};

type OwnProps = {||};

class Exceptions extends Component<Props> {
  render() {
    const { exceptions, selectedSource } = this.props;

    if (!selectedSource || !exceptions.length) {
      return null;
    }

    const doc = getDocument(selectedSource.id);

    return (
      <>
        {exceptions.map(exc => (
          <Exception
            exception={exc}
            doc={doc}
            key={`${exc.sourceActorId}:${exc.lineNumber}`}
          />
        ))}
      </>
    );
  }
}

export default connect<Props, OwnProps, _, _, _, _>(state => ({
  exceptions: getSelectedSourceExceptions(state),
  selectedSource: getSelectedSource(state),
}))(Exceptions);
