/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const ThisFirefox = require("../runtimes/this-firefox");

class App extends PureComponent {
  static get propTypes() {
    return {
      thisFirefox: PropTypes.instanceOf(ThisFirefox).isRequired,
    };
  }

  render() {
    return dom.div(
      {
        className: "app",
      },
    );
  }
}

module.exports = App;
