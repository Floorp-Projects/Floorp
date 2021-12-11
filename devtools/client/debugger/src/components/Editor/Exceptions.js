/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import { connect } from "../../utils/connect";

import Exception from "./Exception";

import {
  getSelectedSource,
  getSelectedSourceExceptions,
} from "../../selectors";
import { getDocument } from "../../utils/editor";

class Exceptions extends Component {
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
            selectedSourceId={selectedSource.id}
          />
        ))}
      </>
    );
  }
}

export default connect(state => ({
  exceptions: getSelectedSourceExceptions(state),
  selectedSource: getSelectedSource(state),
}))(Exceptions);
