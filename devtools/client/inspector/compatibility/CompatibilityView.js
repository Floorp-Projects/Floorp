/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  createElement,
} = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const CompatibilityApp = createFactory(
  require("./components/CompatibilityApp")
);

class CompatibilityView {
  constructor(inspector, window) {
    this._init();
  }

  destroy() {}

  _init() {
    const compatibilityApp = new CompatibilityApp();

    this.provider = createElement(
      Provider,
      {
        id: "compatibilityview",
      },
      compatibilityApp
    );
  }
}

module.exports = CompatibilityView;
