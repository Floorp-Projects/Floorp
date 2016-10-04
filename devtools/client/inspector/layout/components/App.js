/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createClass, DOM: dom } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");

let App = createClass({

  displayName: "App",

  render() {
    return dom.div(
      {
        id: "app",
      }
    );
  },

});

module.exports = connect(state => state)(App);
