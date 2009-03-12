function doubler(k, v) {
  do_check_true("string" == typeof k);

  if ((typeof v) == "number")
    return 2 * v;

  return v;
}

function run_test() {
  var x = JSON.parse('{"a":5,"b":6}', doubler);
  do_check_true(x.hasOwnProperty('a'));
  do_check_true(x.hasOwnProperty('b'));
  do_check_eq(x.a, 10);
  do_check_eq(x.b, 12);

  x = JSON.parse('[3, 4, 5]', doubler);
  do_check_eq(x[0], 6);
  do_check_eq(x[1], 8);
  do_check_eq(x[2], 10);

  // make sure reviver isn't called after a failed parse
  var called = false;
  function dontCallMe(k, v) {
    called = true;
  }

  try {
    x = JSON.parse('{{{{{{{}}}}', dontCallMe);
  } catch (e) {
    if (! (e instanceof SyntaxError))
      throw("wrong exception");
  }

  do_check_false(called);
}
