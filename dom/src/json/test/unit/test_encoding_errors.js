function tooDeep() {
  var arr = [];
  var root = [arr];
  var tail;
  for (var i = 0; i < 5000; i++) {
    tail = [];
    arr.push(tail);
    arr = tail;
  }
  JSON.stringify(root);
}

function run_test() {
  do_check_eq(undefined, JSON.stringify(undefined));
  do_check_eq(undefined, JSON.stringify(function(){}));
  do_check_eq(undefined, JSON.stringify(<x><y></y></x>));

  var ok = false;
  try {
    tooDeep();
  } catch (e) {
    do_check_true(e instanceof Error);
    ok = true;
  }
  do_check_true(ok);
}

