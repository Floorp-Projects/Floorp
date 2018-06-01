/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported waitUntilState */

"use strict";

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});

function waitUntilState(store, predicate) {
  return new Promise(resolve => {
    const unsubscribe = store.subscribe(check);
    function check() {
      if (predicate(store.getState())) {
        unsubscribe();
        resolve();
      }
    }

    // Fire the check immediately incase the action has already occurred
    check();
  });
}
