// Binary: cache/js-dbg-64-e8a025a7101b-linux
// Flags: -m -n
//
var SECTION = "";
function AddTestCase( description, expect, actual ) {}
function TestCase(n, d, e, a) {}
TestCase.prototype.dump = function () {};
function printStatus (msg) {
  msg = msg.toString();
  var lines = msg.split ("\n");
}
function printBugNumber (num) {}
function optionsInit() {}
function optionsClear() {}
  var optionsframe = {};
  try {
    optionsClear();
    for (var optionName in options.initvalues)    {    }
  } catch(ex)  {
  optionsInit();
  if (typeof window == 'undefined' && typeof print == 'function')
    try {
      gTestcases[gTc].passed = writeTestCaseResult(
        gTestcases[gTc].description +" = "+ gTestcases[gTc].actual );
    } catch(e) {
      if (typeof document != "object" || !document.location.href.match(/jsreftest.html/)) {}
    }
  }
  code = "(function(x){ if(x) return x; })";
gczeal(2);
for (var loopa2 = 0; loopa2 < 13; loopa2++) {
  [, , , , , , ][(loopa2.loopa2)] &=  (/\u0042[\u0061]\\u0026/ );
}
this.summary = false;
printStatus (summary);
try {} catch(e) {}
var TIME_0000  = (function () {  })();
function getTimeZoneDiff() {}
new TestCase( SECTION, Array.prototype.reverse.length );
new TestCase( SECTION, eval("delete Array.prototype.reverse.length; Array.prototype.reverse.length") );
var S = "var A = new Array( true,false )";
eval(S);
var R = Reverse(A);
new TestCase( SECTION, eval( S + "; A.reverse(); A.length") );
CheckItems(  R, A );
CheckItems( R, A );
eval(S);
new TestCase( SECTION, eval( S + "; A.reverse(); A.length") );
CheckItems( R, A );
var S = "var A = new Array(); A[8] = 'hi', A[3] = 'yo'";
eval(S);
var R = Reverse(A);
new TestCase( SECTION, eval( S + "; A.reverse(); A.length") );
CheckItems( R, A );
var OBJECT_OBJECT = new Object();
var FUNCTION_OBJECT = new Function( 'return this' );
var BOOLEAN_OBJECT = new Boolean;
var DATE_OBJECT = new Date(0);
var STRING_OBJECT = new String('howdy');
var NUMBER_OBJECT = new Number(Math.PI);
var ARRAY_OBJECT= new Array(1000);
var args = "null, void 0, Math.pow(2,32), 1.234e-32, OBJECT_OBJECT, BOOLEAN_OBJECT, FUNCTION_OBJECT, DATE_OBJECT, STRING_OBJECT,"+
  "ARRAY_OBJECT, NUMBER_OBJECT, Math, true, false, 123, '90210'";
var S = "var A = new Array("+args+")";
eval(S);
var R = Reverse(A);
new TestCase( SECTION, eval( S + "; A.reverse(); A.length") );
CheckItems( R, A );
var limit = 1000;
for (var i = 0; i < limit; i++ ) {
  args += i +"";
}
function CheckItems( R, A ) {
  for ( var i = 0; i < R.length; i++ ) {
    new TestCase( ( code     ) [i] );
  }
}
function Object_1( value ) {}
function Reverse( array ) {
  return array;
}
