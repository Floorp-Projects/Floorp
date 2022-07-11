/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

// Helper to step a generator function and catch a StopIteration exception.
function do_run_generator(generator) {
  try {
    generator.next();
  } catch (e) {
    do_throw("caught exception " + e, Components.stack.caller);
  }
}

// Helper to finish a generator function test.
function do_finish_generator_test(generator) {
  executeSoon(function() {
    generator.return();
    do_test_finished();
  });
}

function do_count_array(all) {
  return all.length;
}
