/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import PropTypes from "prop-types";
import { connect } from "../../utils/connect";

import Exception from "./Exception";

import {
  getSelectedSource,
  getSelectedSourceExceptions,
} from "../../selectors";
import { getDocument } from "../../utils/editor";

class Exceptions extends Component {
  static get propTypes() {
    return {
      exceptions: PropTypes.array,
      selectedSource: PropTypes.object,
    };
  }

  render() {
    const { exceptions, selectedSource } = this.props;

    if (!selectedSource || !exceptions.length) {
      return null;
    }

    const doc = getDocument(selectedSource.id);
    return React.createElement(
      React.Fragment,
      null,
      exceptions.map(exception =>
        React.createElement(Exception, {
          exception,
          doc,
          key: `${exception.sourceActorId}:${exception.lineNumber}`,
          selectedSource,
        })
      )
    );
  }
}

export default connect(state => {
  const selectedSource = getSelectedSource(state);

  // Avoid calling getSelectedSourceExceptions when there is no source selected.
  if (!selectedSource) {
    return {};
  }

  // Avoid causing any update until we start having exceptions
  const exceptions = getSelectedSourceExceptions(state);
  if (!exceptions.length) {
    return {};
  }

  return {
    exceptions: getSelectedSourceExceptions(state),
    selectedSource,
  };
})(Exceptions);
