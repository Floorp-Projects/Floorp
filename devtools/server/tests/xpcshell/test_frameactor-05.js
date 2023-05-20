/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );
    await checkFramesLength(threadFront, 5);

    await resumeAndWaitForPause(threadFront);
    await checkFramesLength(threadFront, 2);

    await threadFront.resume();
  })
);

function evalCode(debuggee) {
  debuggee.eval(
    "(" +
      function () {
        function depth3() {
          debugger;
        }
        function depth2() {
          depth3();
        }
        function depth1() {
          depth2();
        }
        depth1();
        debugger;
      } +
      ")()"
  );
}
