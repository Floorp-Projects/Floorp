// |jit-test| error:InternalError

// Binary: cache/js-dbg-64-d43c6dddeb2b-linux
// Flags: -m -n
//

function printBugNumber (num) {
  BUGNUMBER = num;
  print ('BUGNUMBER: ' + num);
}
var actual = '';
test();
function test() {
  printBugNumber(test);
  function f(N) {
    for (var i = 0; i != N; ++i) {
      for (var repeat = 0;repeat != 2; ++repeat) {
        var count = BUGNUMBER(repeat);
      }
    }
  }
  var array = [function() { f(10); }, ];
  for (var i = 0; i != array.length; ++i)
    array[i]();
}
