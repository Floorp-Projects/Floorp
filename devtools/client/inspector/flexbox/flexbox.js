/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class FlexboxInspector {

  constructor(inspector, window) {
    this.document = window.document;
    this.inspector = inspector;
    this.store = inspector.store;
  }

  destroy() {
    this.document = null;
    this.inspector = null;
    this.store = null;
  }

}

module.exports = FlexboxInspector;
