/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  let ac = new AbortController();
  Assert.ok(ac instanceof AbortController);
  Assert.ok(ac.signal instanceof AbortSignal);
}
