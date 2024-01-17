/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function() {
  // Throwing an error inside a JS callback in xpconnect should preserve
  // the columnNumber.

  const tests = [
    // Parser error.
    {
      throwError() {
        eval("a b");
      },
      messagePattern: /unexpected token/,
      lineNumber: 1,
      columnNumber: 3,
    },
    // Runtime error.
    {
      throwError() { // line = 21
        not_found();
      },
      messagePattern: /is not defined/,
      lineNumber: 22,
      columnNumber: 9,
    },
  ];

  for (const test of tests) {
    const { promise, resolve } = Promise.withResolvers();
    const listener = {
      observe(msg) {
        if (msg instanceof Ci.nsIScriptError) {
          resolve(msg);
        }
      }
    };

    try {
      Services.console.registerListener(listener);

      try {
        const obs = Cc["@mozilla.org/observer-service;1"]
              .getService(Ci.nsIObserverService);
        obs.addObserver(test.throwError, "test-obs", false);
        obs.notifyObservers(null, "test-obs");
      } catch {}

      const msg = await promise;
      Assert.stringMatches(msg.errorMessage, test.messagePattern);
      Assert.equal(msg.lineNumber, test.lineNumber);
      Assert.equal(msg.columnNumber, test.columnNumber);
    } finally {
      Services.console.unregisterListener(listener);
    }
  }
});
