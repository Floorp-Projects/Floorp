/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const React = require("devtools/client/shared/vendor/react");
const DOM = React.DOM;

var DummyChildComponent = React.createClass({
  displayName: "DummyChildComponent",

  render() {
    return (
      DOM.div({}, "DummyChildComponent foobar")
    );
  }
});

// Exports from this module
module.exports = DummyChildComponent;
