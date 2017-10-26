/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createElement } = require("devtools/client/shared/vendor/react");

const App = require("./components/App");

class AnimationInspector {
  constructor() {
    this.init();
  }

  init() {
    this.provider = createElement(App);
  }
}

module.exports = AnimationInspector;
