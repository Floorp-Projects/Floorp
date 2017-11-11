/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  let ac = new AbortController();
  do_check_true(ac instanceof AbortController);
  do_check_true(ac.signal instanceof AbortSignal);
}
