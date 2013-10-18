let a = [];
a.length = 30;

function check(f) {
  try {
    f();
  } catch (e) {
    assertEq(e.message, "too much recursion");
  }
}

let f = function() f(...a) + 1;
let g = () => g(...a) + 1;
let h = function() new h(...a) + 1;

check(f);
check(g);
check(h);
