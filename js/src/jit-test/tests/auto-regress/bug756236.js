// Binary: cache/js-dbg-64-14735b4dbccc-linux
// Flags: --ion-eager
//

gczeal(4);
function startTest() {}
function TestCase(n, d, e, a)
    dump = (function () {});
  if (typeof document != "object" || !document.location.href.match(/jsreftest.html/)) {}
function writeHeaderToLog( string ) {}
var SECTION = "11.4.5";
new TestCase( SECTION,  "var MYVAR= void 0; --MYVAR", NaN, eval("var MYVAR=void 0; --MYVAR") );
new TestCase( SECTION, "var MYVAR=0;--MYVAR;MYVAR", -1, eval("var MYVAR=0;--MYVAR;MYVAR") );
new TestCase( SECTION, "var MYVAR=0;--MYVAR;MYVAR", -1, eval("var MYVAR=0;--MYVAR;MYVAR") );
