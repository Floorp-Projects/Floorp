// |jit-test| error:InternalError

// Binary: cache/js-dbg-32-a9a18824b4c1-linux
// Flags: --ion-eager
//

gczeal(2)
function test() {
  typeof (new test("1")) != 'function'
}
test();
