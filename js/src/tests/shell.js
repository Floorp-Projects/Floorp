/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTE: If you're adding new test harness functionality -- first, should you
//       at all?  Most stuff is better in specific tests, or in nested shell.js
//       or browser.js.  Second, supposing you should, please add it to this
//       IIFE for better modularity/resilience against tests that must do
//       particularly bizarre things that might break the harness.

(function(global) {
  /**********************************************************************
   * CACHED PRIMORDIAL FUNCTIONALITY (before a test might overwrite it) *
   **********************************************************************/

  var undefined; // sigh

  var Error = global.Error;
  var Number = global.Number;
  var TypeError = global.TypeError;

  var ArrayIsArray = global.Array.isArray;
  var ObjectCreate = global.Object.create;
  var ObjectDefineProperty = global.Object.defineProperty;
  var ReflectApply = global.Reflect.apply;
  var StringPrototypeEndsWith = global.String.prototype.endsWith;

  /****************************
   * GENERAL HELPER FUNCTIONS *
   ****************************/

  // We could use Array.prototype.pop, but we don't so it's clear exactly what
  // dependencies this function has on test-modifiable behavior (i.e. none).
  function ArrayPop(arr) {
    assertEq(ArrayIsArray(arr), true,
             "ArrayPop must only be used on actual arrays");

    var len = arr.length;
    if (len === 0)
      return undefined;

    var v = arr[len - 1];
    arr.length--;
    return v;
  }

  // We *cannot* use Array.prototype.push for this, because that function sets
  // the new trailing element, which could invoke a setter (left by a test) on
  // Array.prototype or Object.prototype.
  function ArrayPush(arr, val) {
    assertEq(ArrayIsArray(arr), true,
             "ArrayPush must only be used on actual arrays");

    var desc = ObjectCreate(null);
    desc.value = val;
    desc.enumerable = true;
    desc.configurable = true;
    desc.writable = true;
    ObjectDefineProperty(arr, arr.length, desc);
  }

  function StringEndsWith(str, needle) {
    return ReflectApply(StringPrototypeEndsWith, str, [needle]);
  }

  /****************************
   * TESTING FUNCTION EXPORTS *
   ****************************/

  function SameValue(v1, v2) {
    // We could |return Object.is(v1, v2);|, but that's less portable.
    if (v1 === 0 && v2 === 0)
      return 1 / v1 === 1 / v2;
    if (v1 !== v1 && v2 !== v2)
      return true;
    return v1 === v2;
  }

  var assertEq = global.assertEq;
  if (typeof assertEq !== "function") {
    assertEq = function assertEq(actual, expected, message) {
      if (!SameValue(actual, expected)) {
        throw new TypeError('Assertion failed: got "' + actual + '", ' +
                            'expected "' + expected +
                            (message ? ": " + message : ""));
      }
    };
    global.assertEq = assertEq;
  }

  function assertEqArray(actual, expected) {
    var len = actual.length;
    assertEq(len, expected.length, "mismatching array lengths");

    var i = 0;
    try {
      for (; i < len; i++)
        assertEq(actual[i], expected[i], "mismatch at element " + i);
    } catch (e) {
      throw new Error("Exception thrown at index " + i + ": " + e);
    }
  }
  global.assertEqArray = assertEqArray;

  function assertThrows(f) {
    var ok = false;
    try {
      f();
    } catch (exc) {
      ok = true;
    }
    if (!ok)
      throw new Error("Assertion failed: " + f + " did not throw as expected");
  }
  global.assertThrows = assertThrows;

  function assertThrowsInstanceOf(f, ctor, msg) {
    var fullmsg;
    try {
      f();
    } catch (exc) {
      if (exc instanceof ctor)
        return;
      fullmsg =
        "Assertion failed: expected exception " + ctor.name + ", got " + exc;
    }

    if (fullmsg === undefined) {
      fullmsg =
        "Assertion failed: expected exception " + ctor.name + ", " +
        "no exception thrown";
    }
    if (msg !== undefined)
      fullmsg += " - " + msg;

    throw new Error(fullmsg);
  }
  global.assertThrowsInstanceOf = assertThrowsInstanceOf;

  /****************************
   * UTILITY FUNCTION EXPORTS *
   ****************************/

  // Eventually this polyfill should be defined here, not in browser.js.  For
  // now tolerate more-resilient code depending on less-resilient code.
  assertEq(typeof global.print, "function",
           "print function is pre-existing, either provided by the shell or " +
           "the already-executed top-level browser.js");

  /******************************************************
   * TEST METADATA EXPORTS (these are of dubious value) *
   ******************************************************/

  global.SECTION = "";
  global.VERSION = "";
  global.BUGNUMBER = "";

  /*************************************************************************
   * HARNESS-CENTRIC EXPORTS (we should generally work to eliminate these) *
   *************************************************************************/

  /** Set up test environment. */
  function startTest() {
    if (global.BUGNUMBER)
      global.print("BUGNUMBER: " + global.BUGNUMBER);
  }
  global.startTest = startTest;

  var callStack = [];

  /**
   * Puts funcName at the top of the call stack.  This stack is used to show
   * a function-reported-from field when reporting failures.
   */
  function enterFunc(funcName) {
    assertEq(typeof funcName, "string",
             "enterFunc must be given a string funcName");

    if (!StringEndsWith(funcName, "()"))
      funcName += "()";

    ArrayPush(callStack, funcName);
  }
  global.enterFunc = enterFunc;

  /**
   * Pops the top funcName off the call stack.  funcName, if provided, is used
   * to check push-pop balance.
   */
  function exitFunc(funcName) {
    assertEq(typeof funcName === "string" || typeof funcName === "undefined",
             true,
             "exitFunc must be given no arguments or a string");

    var lastFunc = ArrayPop(callStack);
    assertEq(typeof lastFunc, "string", "exitFunc called too many times");

    if (funcName) {
      if (!StringEndsWith(funcName, "()"))
        funcName += "()";

      if (lastFunc !== funcName) {
        // XXX Eliminate this dependency on global.reportCompare's identity.
        global.reportCompare(funcName, lastFunc,
                             "Test driver failure wrong exit function ");
      }
    }
  }
  global.exitFunc = exitFunc;

  /** Peeks at the top of the call stack. */
  function currentFunc() {
    assertEq(callStack.length > 0, true,
             "must be a current function to examine");

    return callStack[callStack.length - 1];
  }
  global.currentFunc = currentFunc;

  /*****************************************************
   * RHINO-SPECIFIC EXPORTS (are these used any more?) *
   *****************************************************/

  function inRhino() {
    return typeof global.defineClass === "function";
  }
  global.inRhino = inRhino;

  function GetContext() {
    return global.Packages.com.netscape.javascript.Context.getCurrentContext();
  }
  global.GetContext = GetContext;

  function OptLevel(i) {
    i = Number(i);
    var cx = GetContext();
    cx.setOptimizationLevel(i);
  }
  global.OptLevel = OptLevel;
})(this);


var STATUS = "STATUS: ";
var SECT_PREFIX = 'Section ';
var SECT_SUFFIX = ' of test - ';

var gDelayTestDriverEnd = false;

var gTestcases = new Array();
var gTc = gTestcases.length;
var summary = '';
var description = '';
var expected = '';
var actual = '';
var msg = '';

/*
 * constant strings
 */
var GLOBAL = this + '';
var PASSED = " PASSED! ";
var FAILED = " FAILED! ";

var DESCRIPTION;
var EXPECTED;

/*
 * Signals to results.py that the current test case should be considered to
 * have passed if it doesn't throw an exception.
 *
 * When the test suite is run in the browser, this function gets overridden by
 * the same-named function in browser.js.
 */
function testPassesUnlessItThrows() {
  print(PASSED);
}

/*
 * wrapper for test case constructor that doesn't require the SECTION
 * argument.
 */

function AddTestCase( description, expect, actual ) {
  new TestCase( SECTION, description, expect, actual );
}

function TestCase(n, d, e, a)
{
  this.name = n;
  this.description = d;
  this.expect = e;
  this.actual = a;
  this.passed = getTestCaseResult(e, a);
  this.reason = '';
  this.bugnumber = typeof(BUGNUMER) != 'undefined' ? BUGNUMBER : '';
  this.type = (typeof window == 'undefined' ? 'shell' : 'browser');
  ({}).constructor.defineProperty(
    gTestcases,
    gTc++,
    {
      value: this,
      enumerable: true,
      configurable: true,
      writable: true
    }
  );
}

gFailureExpected = false;

TestCase.prototype.dump = function () {
  // let reftest handle error reporting, otherwise
  // output a summary line.
  if (typeof document != "object" ||
      !document.location.href.match(/jsreftest.html/))
  {
    dump('\njstest: ' + this.path + ' ' +
         'bug: '         + this.bugnumber + ' ' +
         'result: '      + (this.passed ? 'PASSED':'FAILED') + ' ' +
         'type: '        + this.type + ' ' +
         'description: ' + toPrinted(this.description) + ' ' +
//       'expected: '    + toPrinted(this.expect) + ' ' +
//       'actual: '      + toPrinted(this.actual) + ' ' +
         'reason: '      + toPrinted(this.reason) + '\n');
  }
};

TestCase.prototype.testPassed = (function TestCase_testPassed() { return this.passed; });
TestCase.prototype.testFailed = (function TestCase_testFailed() { return !this.passed; });
TestCase.prototype.testDescription = (function TestCase_testDescription() { return this.description + ' ' + this.reason; });

function getTestCases()
{
  return gTestcases;
}

/*
 * The test driver searches for such a phrase in the test output.
 * If such phrase exists, it will set n as the expected exit code.
 */
function expectExitCode(n)
{
  print('--- NOTE: IN THIS TESTCASE, WE EXPECT EXIT CODE ' + n + ' ---');
}

/*
 * Statuses current section of a test
 */
function inSection(x)
{
  return SECT_PREFIX + x + SECT_SUFFIX;
}

/*
 * Report a failure in the 'accepted' manner
 */
function reportFailure (msg)
{
  var lines = msg.split ("\n");
  var l;
  var funcName = currentFunc();
  var prefix = (funcName) ? "[reported from " + funcName + "] ": "";

  for (var i=0; i<lines.length; i++)
    print (FAILED + prefix + lines[i]);
}

/*
 * Print a non-failure message.
 */
function printStatus (msg)
{
/* js1_6 had...
   msg = String(msg);
   msg = msg.toString();
*/
  msg = msg.toString();
  var lines = msg.split ("\n");
  var l;

  for (var i=0; i<lines.length; i++)
    print (STATUS + lines[i]);
}

/*
 * Print a bugnumber message.
 */
function printBugNumber (num)
{
  BUGNUMBER = num;
  print ('BUGNUMBER: ' + num);
}

function toPrinted(value)
{
  value = String(value);
  value = value.replace(/\\n/g, 'NL')
               .replace(/\n/g, 'NL')
               .replace(/\\r/g, 'CR')
               .replace(/[^\x20-\x7E]+/g, escapeString);
  return value;
}

function escapeString (str)
{
  var a, b, c, d;
  var len = str.length;
  var result = "";
  var digits = ["0", "1", "2", "3", "4", "5", "6", "7",
                "8", "9", "A", "B", "C", "D", "E", "F"];

  for (var i=0; i<len; i++)
  {
    var ch = str.charCodeAt(i);

    a = digits[ch & 0xf];
    ch >>= 4;
    b = digits[ch & 0xf];
    ch >>= 4;

    if (ch)
    {
      c = digits[ch & 0xf];
      ch >>= 4;
      d = digits[ch & 0xf];

      result += "\\u" + d + c + b + a;
    }
    else
    {
      result += "\\x" + b + a;
    }
  }

  return result;
}

/*
 * Compare expected result to actual result, if they differ (in value and/or
 * type) report a failure.  If description is provided, include it in the
 * failure report.
 */
function reportCompare (expected, actual, description) {
  var expected_t = typeof expected;
  var actual_t = typeof actual;
  var output = "";

  if (typeof description == "undefined")
    description = '';

  if (expected_t != actual_t) {
    output += "Type mismatch, expected type " + expected_t +
      ", actual type " + actual_t + " ";
  }

  if (expected != actual) {
    output += "Expected value '" + toPrinted(expected) +
      "', Actual value '" + toPrinted(actual) + "' ";
  }

  var testcase = new TestCase("unknown-test-name", description, expected, actual);
  testcase.reason = output;

  // if running under reftest, let it handle result reporting.
  if (typeof document != "object" ||
      !document.location.href.match(/jsreftest.html/)) {
    if (testcase.passed)
    {
      print(PASSED + description);
    }
    else
    {
      reportFailure (description + " : " + output);
    }
  }
  return testcase.passed;
}

/*
 * Attempt to match a regular expression describing the result to
 * the actual result, if they differ (in value and/or
 * type) report a failure.  If description is provided, include it in the
 * failure report.
 */
function reportMatch (expectedRegExp, actual, description) {
  var expected_t = "string";
  var actual_t = typeof actual;
  var output = "";

  if (typeof description == "undefined")
    description = '';

  if (expected_t != actual_t) {
    output += "Type mismatch, expected type " + expected_t +
      ", actual type " + actual_t + " ";
  }

  var matches = expectedRegExp.test(actual);
  if (!matches) {
    output += "Expected match to '" + toPrinted(expectedRegExp) +
      "', Actual value '" + toPrinted(actual) + "' ";
  }

  var testcase = new TestCase("unknown-test-name", description, true, matches);
  testcase.reason = output;

  // if running under reftest, let it handle result reporting.
  if (typeof document != "object" ||
      !document.location.href.match(/jsreftest.html/)) {
    if (testcase.passed)
    {
      print(PASSED + description);
    }
    else
    {
      reportFailure (description + " : " + output);
    }
  }
  return testcase.passed;
}

/*
 * An xorshift pseudo-random number generator see:
 * https://en.wikipedia.org/wiki/Xorshift#xorshift.2A
 * This generator will always produce a value, n, where
 * 0 <= n <= 255
 */
function *XorShiftGenerator(seed, size) {
    let x = seed;
    for (let i = 0; i < size; i++) {
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        yield x % 256;
    }
}

function compareSource(expect, actual, summary)
{
  // compare source
  var expectP = expect.
    replace(/([(){},.:\[\]])/mg, ' $1 ').
    replace(/(\w+)/mg, ' $1 ').
    replace(/<(\/)? (\w+) (\/)?>/mg, '<$1$2$3>').
    replace(/\s+/mg, ' ').
    replace(/new (\w+)\s*\(\s*\)/mg, 'new $1');

  var actualP = actual.
    replace(/([(){},.:\[\]])/mg, ' $1 ').
    replace(/(\w+)/mg, ' $1 ').
    replace(/<(\/)? (\w+) (\/)?>/mg, '<$1$2$3>').
    replace(/\s+/mg, ' ').
    replace(/new (\w+)\s*\(\s*\)/mg, 'new $1');

  print('expect:\n' + expectP);
  print('actual:\n' + actualP);

  reportCompare(expectP, actualP, summary);

  // actual must be compilable if expect is?
  try
  {
    var expectCompile = 'No Error';
    var actualCompile;

    eval(expect);
    try
    {
      eval(actual);
      actualCompile = 'No Error';
    }
    catch(ex1)
    {
      actualCompile = ex1 + '';
    }
    reportCompare(expectCompile, actualCompile,
                  summary + ': compile actual');
  }
  catch(ex)
  {
  }
}

function optionsInit() {

  // record initial values to support resetting
  // options to their initial values
  options.initvalues  = {};

  // record values in a stack to support pushing
  // and popping options
  options.stackvalues = [];

  var optionNames = options().split(',');

  for (var i = 0; i < optionNames.length; i++)
  {
    var optionName = optionNames[i];
    if (optionName)
    {
      options.initvalues[optionName] = '';
    }
  }
}

function optionsClear() {

  // turn off current settings
  // except jit.
  var optionNames = options().split(',');
  for (var i = 0; i < optionNames.length; i++)
  {
    var optionName = optionNames[i];
    if (optionName &&
        optionName != "methodjit" &&
        optionName != "methodjit_always" &&
        optionName != "ion")
    {
      options(optionName);
    }
  }
}

function optionsPush()
{
  var optionsframe = {};

  options.stackvalues.push(optionsframe);

  var optionNames = options().split(',');

  for (var i = 0; i < optionNames.length; i++)
  {
    var optionName = optionNames[i];
    if (optionName)
    {
      optionsframe[optionName] = '';
    }
  }

  optionsClear();
}

function optionsPop()
{
  var optionsframe = options.stackvalues.pop();

  optionsClear();

  for (optionName in optionsframe)
  {
    options(optionName);
  }

}

function optionsReset() {

  try
  {
    optionsClear();

    // turn on initial settings
    for (var optionName in options.initvalues)
    {
      if (!options.hasOwnProperty(optionName))
        continue;
      options(optionName);
    }
  }
  catch(ex)
  {
    print('optionsReset: caught ' + ex);
  }

}

if (typeof options == 'function')
{
  optionsInit();
  optionsClear();
}

function getTestCaseResult(expected, actual)
{
  if (typeof expected != typeof actual)
    return false;
  if (typeof expected != 'number')
    // Note that many tests depend on the use of '==' here, not '==='.
    return actual == expected;

  // Distinguish NaN from other values.  Using x != x comparisons here
  // works even if tests redefine isNaN.
  if (actual != actual)
    return expected != expected;
  if (expected != expected)
    return false;

  // Tolerate a certain degree of error.
  if (actual != expected)
    return Math.abs(actual - expected) <= 1E-10;

  // Here would be a good place to distinguish 0 and -0, if we wanted
  // to.  However, doing so would introduce a number of failures in
  // areas where they don't seem important.  For example, the WeekDay
  // function in ECMA-262 returns -0 for Sundays before the epoch, but
  // the Date functions in SpiderMonkey specified in terms of WeekDay
  // often don't.  This seems unimportant.
  return true;
}

if (typeof dump == 'undefined')
{
  if (typeof window == 'undefined' &&
      typeof print == 'function')
  {
    dump = print;
  }
  else
  {
    dump = (function () {});
  }
}

function test() {
  for ( gTc=0; gTc < gTestcases.length; gTc++ ) {
    // temporary hack to work around some unknown issue in 1.7
    try
    {
      gTestcases[gTc].passed = writeTestCaseResult(
        gTestcases[gTc].expect,
        gTestcases[gTc].actual,
        gTestcases[gTc].description +" = "+ gTestcases[gTc].actual );
      gTestcases[gTc].reason += ( gTestcases[gTc].passed ) ? "" : "wrong value ";
    }
    catch(e)
    {
      print('test(): empty testcase for gTc = ' + gTc + ' ' + e);
    }
  }
  stopTest();
  return ( gTestcases );
}

/*
 * Begin printing functions.  These functions use the shell's
 * print function.  When running tests in the browser, these
 * functions, override these functions with functions that use
 * document.write.
 */

function writeTestCaseResult( expect, actual, string ) {
  var passed = getTestCaseResult( expect, actual );
  // if running under reftest, let it handle result reporting.
  if (typeof document != "object" ||
      !document.location.href.match(/jsreftest.html/)) {
    writeFormattedResult( expect, actual, string, passed );
  }
  return passed;
}
function writeFormattedResult( expect, actual, string, passed ) {
  var s = ( passed ? PASSED : FAILED ) + string + ' expected: ' + expect;
  print(s);
  return passed;
}

function writeHeaderToLog( string ) {
  print( string );
}
/* end of print functions */


/*
 * When running in the shell, run the garbage collector after the
 * test has completed.
 */

function stopTest() {
  var gc;
  if ( gc != undefined ) {
    gc();
  }
}

function jsTestDriverEnd()
{
  // gDelayTestDriverEnd is used to
  // delay collection of the test result and
  // signal to Spider so that tests can continue
  // to run after page load has fired. They are
  // responsible for setting gDelayTestDriverEnd = true
  // then when completed, setting gDelayTestDriverEnd = false
  // then calling jsTestDriverEnd()

  if (gDelayTestDriverEnd)
  {
    return;
  }

  try
  {
    optionsReset();
  }
  catch(ex)
  {
    dump('jsTestDriverEnd ' + ex);
  }

  for (var i = 0; i < gTestcases.length; i++)
  {
    gTestcases[i].dump();
  }

}
