/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Simple controller wrapper that binds a controller and runs cleanup function
// https://lit.dev/docs/composition/controllers/
export const withSimpleController = (host, functionToBind, ...args) =>
  class {
    constructor() {
      host.addController(this);
    }
    hostConnected() {
      this.cleanup = functionToBind?.(host, ...args);
    }
    hostDisconnected() {
      this.cleanup?.();
    }
  };
