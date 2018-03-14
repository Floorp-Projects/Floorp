/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const { div } = require("devtools/client/shared/vendor/react-dom-factories");

class App extends Component {
  render() {
    return (
      div({className: "application"}, "application panel content")
    );
  }
}

module.exports = App;
