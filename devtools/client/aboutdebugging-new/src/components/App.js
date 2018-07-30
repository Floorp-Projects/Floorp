/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const Sidebar = createFactory(require("./Sidebar"));

class App extends PureComponent {
  render() {
    return dom.div(
      {
        className: "app",
      },
      Sidebar()
    );
  }
}

module.exports = App;
