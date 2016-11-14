/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getStr } = require("../utils/l10n");
const { DOM: dom, createClass, createFactory } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const Accordion = createFactory(require("./Accordion"));
const Grid = createFactory(require("./Grid"));

const App = createClass({

  displayName: "App",

  render() {
    return dom.div(
      {
        id: "layoutview-container",
      },
      Accordion({
        items: [
          { header: getStr("layout.header"),
            component: Grid,
            opened: true }
        ]
      })
    );
  },

});

module.exports = connect(state => state)(App);
