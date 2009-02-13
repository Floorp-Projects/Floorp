function stringify_primitives() {
  // sanity
  var x = JSON.stringify({});
  do_check_eq(x, "{}");

  // booleans and null
  x = JSON.stringify(true);
  do_check_eq(x, "true");

  x = JSON.stringify(false);
  do_check_eq(x, "false");

  x = JSON.stringify(new Boolean(false));
  do_check_eq(x, "false");

  x = JSON.stringify(null);
  do_check_eq(x, "null");

  x = JSON.stringify(1234);
  do_check_eq(x, "1234");

  x = JSON.stringify(new Number(1234));
  do_check_eq(x, "1234");

  x = JSON.stringify("asdf");
  do_check_eq(x, '"asdf"');

  x = JSON.stringify(new String("asdf"));
  do_check_eq(x, '"asdf"');
}

function run_test() {
  stringify_primitives();
}
