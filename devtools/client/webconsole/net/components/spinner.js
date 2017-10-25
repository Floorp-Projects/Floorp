/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component, DOM } = require("devtools/client/shared/vendor/react");

/**
 * This template represents a throbber displayed when the UI
 * is waiting for data coming from the backend (debugging server).
 */
class Spinner extends Component {
  render() {
    return (
      DOM.div({className: "devtools-throbber"})
    );
  }
}

// Exports from this module
module.exports = Spinner;
