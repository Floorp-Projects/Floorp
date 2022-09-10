/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function worker() {
  await run_test_in_worker("worker/test_syncAccessHandle_worker.js");
});
