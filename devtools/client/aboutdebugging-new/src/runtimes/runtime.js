/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This class represents a runtime, such as a remote Firefox.
 */
class Runtime {
  /**
   * Return icon of this runtime on sidebar.
   * Subclass should override this method.
   * @return {String}
   */
  getIcon() {
    throw new Error("Subclass of Runtime should override getIcon()");
  }

  /**
   * Return name of this runtime on sidebar.
   * Subclass should override this method.
   * @return {String}
   */
  getName() {
    throw new Error("Subclass of Runtime should override getName()");
  }
}

module.exports = Runtime;
