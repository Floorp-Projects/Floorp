/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URI =
  "chrome://mochitests/content/browser/devtools/client/shared/sourceeditor/" +
  "test/codemirror/codemirror.html";

add_task(async function test() {
  requestLongerTimeout(3);

  /*
   * In devtools/client/shared/sourceeditor/test/codemirror/search_test.js there is a test
   * multilineInsensitiveSlow which assumes an operation takes less than 100ms.
   * With a precision of 100ms, if we get unlikely and begin execution towards the
   * end of one spot (e.g. at 95 ms) we will clamp down, take (e.g.) 10ms to execute
   * and it will appear to take 100ms.
   *
   * To avoid this, we hardcode to 2ms of precision.
   *
   * In theory we don't need to set the pref for all of CodeMirror, in practice
   * it seems very difficult to set a pref for just one of the tests.
   */
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.reduceTimerPrecision", true],
      ["privacy.resistFingerprinting.reduceTimerPrecision.microseconds", 2000],
    ],
  });

  await runCodeMirrorTest(URI);
});
