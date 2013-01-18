// |jit-test| error:InternalError

// Binary: cache/js-dbg-64-3062ff7fef83-linux
// Flags: -m -n
//
function enterFunc (funcName) {}
test();
function test()
{
  enterFunc ( "NaN number", "'eeeefghi'.match(/e*/)", new test( "Date.prototype" ));
  function err() {
    return new Error();
  }
  var error = err(s,s,s,s);
}
