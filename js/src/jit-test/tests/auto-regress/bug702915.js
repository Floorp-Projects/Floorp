// Binary: cache/js-dbg-64-d51bd1645a2f-linux
// Flags: -m -n -a
//
gczeal(4);
var callStack = new Array();
var gTestcases = new Array();
var gTc = gTestcases.length;
function TestCase(n, d, e, a) {
  this.name = n;
  this.description = d;
  this.expect = e;
  this.actual = a;
  this.passed = getTestCaseResult(e, a);
  this.reason = '';
  this.bugnumber = '';
  this.type = (typeof window == 'undefined' ? 'shell' : 'browser');
  gTestcases[gTc++] = this;
}
function enterFunc (funcName) {
  try { throw foo; } catch(ex) {}
}
function getTestCaseResult(expected, actual) {}
function writeHeaderToLog( string ) {}
var lfcode = new Array();
lfcode.push("\
	var code = ''; \
	code+=createCode((gczeal(2))); \
	function createCode(i) { \
	  jstop+= +  delete   + i + \" string.';\"; \
	} \
");
lfcode.push("\
var SECTION = '9.9-1'; \
new TestCase( SECTION, '(Object(true)).__proto__',  Boolean.prototype,      (Object(true)).__proto__ ); \
new TestCase( SECTION, '(Object(1)).__proto__',     Number.prototype,      (Object(1)).__proto__ ); \
new TestCase( SECTION, '(Object(-1)).__proto__',    Number.prototype,      (Object(-1)).__proto__ ); \
new TestCase( SECTION, '(Object(Number.MAX_VALUE)).__proto__',  Number.prototype,               (Object(Number.MAX_VALUE)).__proto__ ); \
new TestCase( SECTION, '(Object(Number.MIN_VALUE)).__proto__',  Number.prototype, (Object(Number.MIN_VALUE)).__proto__ ); \
new TestCase( SECTION, '(Object(Number.POSITIVE_INFINITY)).__proto__',  Number.prototype,               (Object(Number.POSITIVE_INFINITY)).__proto__ ); \
new TestCase( SECTION, '(Object(Number.NEGATIVE_INFINITY)).__proto__',  Number.prototype,   (Object(Number.NEGATIVE_INFINITY)).__proto__ ); \
new TestCase( SECTION, '(Object(Number.NaN)).__proto__',    Number.prototype,          (Object(Number.NaN)).__proto__ ); \
new TestCase(  ) ; \
new TestCase( SECTION, '(Object(\"\")).__proto__',   String.prototype,   (Object('')).__proto__ ); \
new TestCase( SECTION, \"(Object('foo')).__proto__\", String.prototype,   (Object('foo')).__proto__ ); \
new TestCase( SECTION,  \"Object( '' ).__proto__\",      String.prototype,   (Object(\"\")).__proto__ ); \
new TestCase( SECTION, '(Object( new MyObject(true) )).toString()',  '[object Object]',       eval('(Object( new MyObject(true) )).toString()') ); \
");
lfcode.push("jsTestDriverEnd();");
lfcode.push("");
lfcode.push("\
	enterFunc ('test'); \
	Array.prototype[1] = 'bar'; \
	var a = [];  \
	exitFunc ('test'); \
");
lfcode.push("");
lfcode.push("\
  var VERSION = 'ECMA_1'; \
var TITLE   = 'Value Properties of the Math Object'; \
writeHeaderToLog( SECTION + ' '+ TITLE); \
new TestCase( '15.8.1.1', 'Math.E',             \
              this  . TITLE     ); \
              'typeof Math.E',      \
new TestCase( '15.8.1.2', \
              'Math.LN10',          \
              'typeof Math.LN10',   \
              typeof Math.LN10 ); \
new TestCase( '15.8.1.3', \
              'Math.LN2',          \
              Math.LN2 ); \
new TestCase( '15.8.1.3', \
              Math.LOG2E ); \
new TestCase( '15.8.1.4', \
              Math.SQRT1_2); \
new TestCase( '15.8.1.7', \
              typeof Math.SQRT2 ); \
new TestCase( SECTION,  \
              eval('var MATHPROPS=\"\";for( p in Math ){ MATHPROPS +=p; };MATHPROPS') ); \
");
while (true) {
	var file = lfcode.shift(); if (file == undefined) { break; }
		try {
				eval(file);
		} catch(exc1) { print(exc1); };
}
