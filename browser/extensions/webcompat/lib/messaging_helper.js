/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser */

// By default, only the first handler for browser.runtime.onMessage which
// returns a value will get to return one. As such, we need to let them all
// receive the message, and all have a chance to return a response (with the
// first non-undefined result being the one that is ultimately returned).
// This way, about:compat and the shims library can both get a chance to
// process a message, and just return undefined if they wish to ignore it.

const onMessageFromTab = (function() {
  const handlers = new Set();

  browser.runtime.onMessage.addListener((msg, sender) => {
    const promises = [...handlers.values()].map(fn => fn(msg, sender));
    return Promise.allSettled(promises).then(results => {
      for (const { reason, value } of results) {
        if (reason) {
          console.error(reason);
        } else if (value !== undefined) {
          return value;
        }
      }
      return undefined;
    });
  });

  return function(handler) {
    handlers.add(handler);
  };
})();
