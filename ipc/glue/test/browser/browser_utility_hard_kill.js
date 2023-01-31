/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async () => {
  await startUtilityProcess(["unknown"]);

  SimpleTest.expectChildProcessCrash();

  info("Hard kill Utility Process");
  await cleanUtilityProcessShutdown("unknown", true /* preferKill */);
});
