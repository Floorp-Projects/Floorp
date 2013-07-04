// |jit-test| slow; error:InternalError

// Binary: cache/js-dbg-32-343ec916dfd5-linux
// Flags: -m -n
//

function TestCase(n, d, e, a)
TestCase.prototype.dump = function () {};
function enterFunc (funcName)
function writeHeaderToLog( string ) {}
gczeal(2);
function f() {}
try {
var BUGNUMBER = 350621;
test();
} catch(exc1) {}
function test() {
  enterFunc ( summary =  this, test(BUGNUMBER));
  function gen1() {  }
  function test_it(RUNS) {  }
}
new TestCase (String([(1),'a22','a23','a24']),
    String('a11\na22\na23\na24'.match(new RegExp('a..$','g'))));
test();
