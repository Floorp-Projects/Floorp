/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that LSNG is not accidentally disabled which
 * can lead to a data loss in a combination with disabled shadow writes.
 */

add_task(async function testSteps() {
  ok(Services.domStorageManager.nextGenLocalStorageEnabled, "LSNG enabled");
});
