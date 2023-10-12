// Binary: cache/js-dbg-32-a9a18824b4c1-linux
// Flags: --ion-eager
//

var i = 0;

gczeal(2);
function test() {
  // Recursion depth reduced to allow PBL with debug build (hence larger
  // frames) to work.
  if (i++ > 75)
    return "function";
  var res = typeof (new test("1")) != 'function';
  return res ? "function" : "string";
}

test();
