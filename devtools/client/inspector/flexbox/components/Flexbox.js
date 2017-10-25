/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  DOM: dom,
  PureComponent,
} = require("devtools/client/shared/vendor/react");

class Flexbox extends PureComponent {
  static get propTypes() {
    return {};
  }

  render() {
    return dom.div(
      {
        id: "layout-flexbox-container",
      }
    );
  }
}

module.exports = Flexbox;
