/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");
const {
  customHighlighterSpec,
} = require("resource://devtools/shared/specs/highlighters.js");
const {
  safeAsyncMethod,
} = require("resource://devtools/shared/async-utils.js");

class CustomHighlighterFront extends FrontClassWithSpec(customHighlighterSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // show/hide requests can be triggered while DevTools are closing.
    this.show = safeAsyncMethod(this.show.bind(this), () => this.isDestroyed());
    this.hide = safeAsyncMethod(this.hide.bind(this), () => this.isDestroyed());

    this._isShown = false;
  }

  show(...args) {
    this._isShown = true;
    return super.show(...args);
  }

  hide() {
    this._isShown = false;
    return super.hide();
  }

  isShown() {
    return this._isShown;
  }

  destroy() {
    if (this.isDestroyed()) {
      return;
    }
    super.finalize(); // oneway call, doesn't expect a response.
    super.destroy();
  }
}

exports.CustomHighlighterFront = CustomHighlighterFront;
registerFront(CustomHighlighterFront);
