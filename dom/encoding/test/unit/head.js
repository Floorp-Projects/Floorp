/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * This is a shim for the W3C testharness.js, mapping those of
 * its functions that we need to the testing/xpcshell/head.js API.
 * See <http://www.w3.org/2008/webapps/wiki/Harness> for documentation.
 * This shim does some tests a little differently than the W3C test
 * harness; equality comparisons, especially, are less precise.
 * The difference does not presently affect any test results.
 *
 * We use the lower-level do_report_result throughout this file,
 * rather than the high-level xpcshell/head.js API that has near
 * equivalents for the W3C assert_* functions, because only
 * do_report_result allows us to provide Components.stack.caller.
 */

function assert_equals(a, b, msg) {
  let text = msg + ": " + _wrap_with_quotes_if_necessary(a) +
                 " == " + _wrap_with_quotes_if_necessary(b);
  do_report_result(a == b, text, Components.stack.caller, false);
}

function assert_not_equals(a, b, msg) {
  let text = msg + ": " + _wrap_with_quotes_if_necessary(a) +
                 " != " + _wrap_with_quotes_if_necessary(b);
  do_report_result(a != b, text, Components.stack.caller, false);
}

function assert_array_equals(a, b, msg) {
  do_report_result(a.length == b.length,
                   msg + ": (length) " + a.length + " == " + b.length,
                   Components.stack.caller, false);
  for (let i = 0; i < a.length; ++i) {
    if (a[i] !== b[i]) {
        do_report_result(false,
                         msg + ": [" + i + "] " +
                               _wrap_with_quotes_if_necessary(a[i]) +
                               " === " +
                               _wrap_with_quotes_if_necessary(b[i]),
                         Components.stack.caller, false);
    }
  }
  // If we get here, all array elements are equal.
  do_report_result(true, msg + ": all array elements equal",
                   Components.stack.caller, false);
}

function assert_true(cond, msg) {
  do_report_result(!!cond, msg + ": " + uneval(cond),
                   Components.stack.caller, false);
}

function assert_throws(ex, func) {
  if (!('name' in ex))
    do_throw("first argument to assert_throws must be of the form " +
             "{'name': something}");

  let msg = "expected to catch an exception named " + ex.name;

  try {
    func();
  } catch (e) {
    if ('name' in e)
      do_report_result(e.name == ex.name,
                       msg + ", got " + e.name,
                       Components.stack.caller, false);
    else
      do_report_result(false,
                       msg + ", got " + legible_exception(ex),
                       Components.stack.caller, false);

    return;
  }

  // Call this here, not in the 'try' clause, so do_report_result's own
  // throw doesn't get caught by our 'catch' clause.
  do_report_result(false, msg + ", but returned normally",
                   Components.stack.caller, false);
}

let tests = [];

function test(func, msg) {
  tests.push({msg: msg, func: func,
              filename: Components.stack.caller.filename });
}

function run_test() {
  tests.forEach(function(t) {
    do_print("test group: " + t.msg,
             {source_file: t.filename});
    t.func();
  });
};
