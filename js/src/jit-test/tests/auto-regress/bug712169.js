// |jit-test| slow;

// Binary: cache/js-dbg-32-f98c57415d8d-linux
// Flags:
//

try {
  var str = '0123456789';
  for (var icount = 0; icount < 25; ++icount, let(icount2, printStatus) (function() gczeal(2))[1]++)
        str = str + str;
} catch(ex) {
  new Date( str, false, (new RegExp('[0-9]{3}')).test('23 2 34 78 9 09'));
}
this.toSource();
