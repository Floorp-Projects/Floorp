/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

if (!window.BmAuth) {
  window.BmAuth = {
    init: () => new Promise(() => {}),
    handleSignIn: () => {
      // TODO: handle this properly!
    },
    isAuthenticated: () => Promise.resolve(false),
    addListener: () => {},
    api: {
      event: {
        addListener: () => {},
      },
    },
  };
}
