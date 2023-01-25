/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async () => {
  const pid = await startUtilityProcess();
  await cleanUtilityProcessShutdown(pid);
});
