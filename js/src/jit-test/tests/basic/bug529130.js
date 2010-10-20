// don't crash

var q = 30;

function var_iter(v) {
  q--;
  yield v;
  if (q > 0) {
    for each (let ret in var_iter(v)) {
      var_iter(v);
      yield ret;
    }
  }
}

for (x in var_iter([1, 2, 3, 4, 5, 6, 7, 8, 9]))
 print(x);


