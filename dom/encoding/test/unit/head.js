/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { 'classes': Cc, 'interfaces': Ci } = Components;

function equal(a, b, msg) {
  dump("equal(" + a + ", " + b + ", \"" + msg + "\")");
  do_check_eq(a, b, Components.stack.caller);
}

function ok(cond, msg) {
  dump("ok(" + cond + ", \"" + msg + "\")");
  do_check_true(!!cond, Components.stack.caller); 
}

function raises(func) {
  dump("raises(" + func + ")");
  try {
    func();
    do_check_true(false, Components.stack.caller); 
  } catch (e) {
    do_check_true(true, Components.stack.caller); 
  }
}

var tests = [];

function test(msg, no, func) {
  tests.push({msg: msg, func: func || no});
}

function expect(count) {
  dump("expect(", count, ")");
}

function run_test() {
  tests.forEach(function(t) {
    dump("test(\"" + t.msg + "\")");
    t.func();
  });
};
