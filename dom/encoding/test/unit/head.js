/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { 'classes': Cc, 'interfaces': Ci } = Components;

// Instantiate nsIDOMScriptObjectFactory so that DOMException is usable in xpcshell
Components.classesByID["{9eb760f0-4380-11d2-b328-00805f8a3859}"].getService(Ci.nsISupports);

function assert_equals(a, b, msg) {
  dump("assert_equals(" + a + ", " + b + ", \"" + msg + "\")");
  do_check_eq(a, b, Components.stack.caller);
}

function assert_not_equals(a, b, msg) {
  dump("assert_not_equals(" + a + ", " + b + ", \"" + msg + "\")");
  do_check_neq(a, b, Components.stack.caller);
}

function assert_array_equals(a, b, msg) {
  dump("assert_array_equals(\"" + msg + "\")");
  do_check_eq(a.length, b.length, Components.stack.caller);
  for (var i = 0; i < a.length; ++i) {
    if (a[i] !== b[i]) {
      do_check_eq(a[i], b[i], Components.stack.caller);
    }
  }
}

function assert_true(cond, msg) {
  dump("assert_true(" + cond + ", \"" + msg + "\")");
  do_check_true(!!cond, Components.stack.caller); 
}

function assert_throws(ex, func) {
  dump("assert_throws(\"" + ex + "\", " + func + ")");
  try {
    func();
    do_check_true(false, Components.stack.caller); 
  } catch (e) {
    do_check_eq(e.name, ex.name, Components.stack.caller); 
  }
}

var tests = [];

function test(func, msg) {
  tests.push({msg: msg, func: func});
}

function run_test() {
  tests.forEach(function(t) {
    dump("test(\"" + t.msg + "\")");
    t.func();
  });
};
