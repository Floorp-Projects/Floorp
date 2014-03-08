gcPreserveCode();
function test() {
  for (var i=0; i<20; i++) {
      arguments.x = {};
      gc();
  }
}
test();
