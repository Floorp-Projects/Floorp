// Binary: cache/js-dbg-32-a9a18824b4c1-linux
// Flags: --ion-eager
//

var i = 0;

gczeal(2);
function test() {
  if (i++ > 2500)
    return "function";
  var res = typeof (new test("1")) != 'function';
  return res ? "function" : "string";
}

test();
