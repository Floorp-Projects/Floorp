// |jit-test| error:InternalError; slow;

// Binary: cache/js-dbg-64-c60535115ea1-linux
// Flags:
//

gczeal(2);
test();
function test() {
  function t(o, proplist) {
    var props=proplist.split(/\s+/g);
  };
  t({ bar: 123, baz: 123, quux: 123 }, 'bar baz quux');
  ( test(), this )(( "7.4.2-6-n" ) , actual, summary + ' : nonjit');
}
