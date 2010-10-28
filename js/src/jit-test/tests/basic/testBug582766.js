expected = 4;

var fourth = { nextSibling: null };
var third  = { nextSibling: fourth };
var second = { nextSibling: third };
var first  = { nextSibling: second };

function f() {
  let loopcount = 0;
  for (let node = first; node; node = node.nextSibling) {
    loopcount++;
  }
  return loopcount;
}

actual = f();

assertEq(actual, expected);
