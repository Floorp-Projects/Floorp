/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const React = require("devtools/client/shared/vendor/react");

// Shortcuts
const DOM = React.DOM;

/**
 * This template represents a throbber displayed when the UI
 * is waiting for data coming from the backend (debugging server).
 */
var Spinner = React.createClass({
  displayName: "Spinner",

  render() {
    return (
      DOM.div({className: "devtools-throbber"})
    );
  }
});

// Exports from this module
module.exports = Spinner;
