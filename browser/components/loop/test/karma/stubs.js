/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Used for desktop coverage tests because triggering methods on
// DOMContentLoaded proved to lead to race conditions.

sinon.stub(document, "addEventListener");
console.log("[stubs.js] addEventListener stubbed to prevent race conditions");
