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
  "use strict";

  /**********************************************************************
   * CACHED PRIMORDIAL FUNCTIONALITY (before a test might overwrite it) *
   **********************************************************************/

  var undefined; // sigh

  var document = global.document;
  var Error = global.Error;
  var Function = global.Function;
  var Number = global.Number;
  var RegExp = global.RegExp;
  var String = global.String;
  var Symbol = global.Symbol;
  var TypeError = global.TypeError;

  var ArrayIsArray = global.Array.isArray;
  var MathAbs = global.Math.abs;
  var ObjectCreate = global.Object.create;
  var ObjectDefineProperty = global.Object.defineProperty;
  var ReflectApply = global.Reflect.apply;
  var RegExpPrototypeExec = global.RegExp.prototype.exec;
  var StringPrototypeCharCodeAt = global.String.prototype.charCodeAt;
  var StringPrototypeEndsWith = global.String.prototype.endsWith;
  var StringPrototypeIndexOf = global.String.prototype.indexOf;
  var StringPrototypeSubstring = global.String.prototype.substring;

  var runningInBrowser = typeof global.window !== "undefined";
  if (runningInBrowser) {
    // Certain cached functionality only exists (and is only needed) when
    // running in the browser.  Segregate that caching here.

    var SpecialPowersSetGCZeal =
      global.SpecialPowers ? global.SpecialPowers.setGCZeal : undefined;
  }

  var runningInShell = typeof window === "undefined";

  var isReftest = typeof document === "object" && /jsreftest.html/.test(document.location.href);

  var evaluate = global.evaluate;

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

  function StringCharCodeAt(str, index) {
    return ReflectApply(StringPrototypeCharCodeAt, str, [index]);
  }

  function StringEndsWith(str, needle) {
    return ReflectApply(StringPrototypeEndsWith, str, [needle]);
  }

  function StringReplace(str, regexp, replacement) {
    assertEq(typeof str === "string" && typeof regexp === "object" &&
             typeof replacement === "function", true,
             "StringReplace must be called with a string, a RegExp object and a function");

    regexp.lastIndex = 0;

    var result = "";
    var last = 0;
    while (true) {
      var match = ReflectApply(RegExpPrototypeExec, regexp, [str]);
      if (!match) {
        result += ReflectApply(StringPrototypeSubstring, str, [last]);
        return result;
      }

      result += ReflectApply(StringPrototypeSubstring, str, [last, match.index]);
      result += ReflectApply(replacement, null, match);
      last = match.index + match[0].length;
    }
  }

  function StringSplit(str, delimiter) {
    assertEq(typeof str === "string" && typeof delimiter === "string", true,
             "StringSplit must be called with two string arguments");
    assertEq(delimiter.length > 0, true,
             "StringSplit doesn't support an empty delimiter string");

    var parts = [];
    var last = 0;
    while (true) {
      var i = ReflectApply(StringPrototypeIndexOf, str, [delimiter, last]);
      if (i < 0) {
        if (last < str.length)
          ArrayPush(parts, ReflectApply(StringPrototypeSubstring, str, [last]));
        return parts;
      }

      ArrayPush(parts, ReflectApply(StringPrototypeSubstring, str, [last, i]));
      last = i + delimiter.length;
    }
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

  var dump = global.dump;
  if (typeof global.dump === "function") {
    // A presumptively-functional |dump| exists, so no need to do anything.
  } else {
    // We don't have |dump|.  Try to simulate the desired effect another way.
    if (runningInBrowser) {
      // We can't actually print to the console: |global.print| invokes browser
      // printing functionality here (it's overwritten just below), and
      // |global.dump| isn't a function that'll dump to the console (presumably
      // because the preference to enable |dump| wasn't set).  Just make it a
      // no-op.
      dump = function() {};
    } else {
      // |print| prints to stdout: make |dump| do likewise.
      dump = global.print;
    }
    global.dump = dump;
  }

  var print;
  if (runningInBrowser) {
    // We're executing in a browser.  Using |global.print| would invoke browser
    // printing functionality: not what tests want!  Instead, use a print
    // function that syncs up with the test harness and console.
    print = function print() {
      var s = "TEST-INFO | ";
      for (var i = 0; i < arguments.length; i++)
        s += String(arguments[i]) + " ";

      // Dump the string to the console for developers and the harness.
      dump(s + "\n");

      // AddPrintOutput doesn't require HTML special characters be escaped.
      global.AddPrintOutput(s);
    };

    global.print = print;
  } else {
    // We're executing in a shell, and |global.print| is the desired function.
    print = global.print;
  }

  var quit = global.quit;
  if (typeof quit !== "function") {
    // XXX There's something very strange about quit() in browser runs being a
    //     function that doesn't quit at all (!).  We should get rid of quit()
    //     as an integral part of tests in favor of something else.
    quit = function quit() {};
    global.quit = quit;
  }

  var gczeal = global.gczeal;
  if (typeof gczeal !== "function") {
    if (typeof SpecialPowersSetGCZeal === "function") {
      gczeal = function gczeal(z) {
        SpecialPowersSetGCZeal(z);
      };
    } else {
      gczeal = function() {}; // no-op if not available
    }

    global.gczeal = gczeal;
  }

  // Evaluates the given source code as global script code. browser.js provides
  // a different implementation for this function.
  var evaluateScript = global.evaluateScript;
  if (typeof evaluate === "function" && typeof evaluateScript !== "function") {
    evaluateScript = function evaluateScript(code) {
      evaluate(String(code));
    };

    global.evaluateScript = evaluateScript;
  }

  // XXX: This function is only exported for a single test file, we should
  // consider to remove its use in that file.
  function toPrinted(value) {
    value = String(value);

    var digits = "0123456789ABCDEF";
    var result = "";
    for (var i = 0; i < value.length; i++) {
      var ch = StringCharCodeAt(value, i);
      if (ch === 0x5C && i + 1 < value.length) {
        var d = value[i + 1];
        if (d === "n") {
          result += "NL";
          i++;
        } else if (d === "r") {
          result += "CR";
          i++;
        } else {
          result += "\\";
        }
      } else if (c === 0x0A) {
        result += "NL";
      } else if (ch < 0x20 || ch > 0x7E) {
        var a = digits[ch & 0xf];
        ch >>= 4;
        var b = digits[ch & 0xf];
        ch >>= 4;

        if (ch) {
          var c = digits[ch & 0xf];
          ch >>= 4;
          var d = digits[ch & 0xf];

          result += "\\u" + d + c + b + a;
        } else {
          result += "\\x" + b + a;
        }
      } else {
        result += value[i];
      }
    }

    return result;
  }
  global.toPrinted = toPrinted;

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
  global.XorShiftGenerator = XorShiftGenerator;

  /******************************************************
   * TEST METADATA EXPORTS (these are of dubious value) *
   ******************************************************/

  global.SECTION = "";
  global.VERSION = "";
  global.BUGNUMBER = "";

  /*************************************************************************
   * HARNESS-CENTRIC EXPORTS (we should generally work to eliminate these) *
   *************************************************************************/

  var PASSED = " PASSED! ";
  global.PASSED = PASSED;

  var FAILED = " FAILED! ";
  global.FAILED = FAILED;

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
    if (callStack.length == 0)
      return "top level script";

    return callStack[callStack.length - 1];
  }
  global.currentFunc = currentFunc;

  // XXX This function is *only* used in harness functions and really shouldn't
  //     be exported.
  var writeFormattedResult =
    function writeFormattedResult(expect, actual, string, passed) {
      print((passed ? PASSED : FAILED) + string + ' expected: ' + expect);
    };
  global.writeFormattedResult = writeFormattedResult;

  /*
   * wrapper for test case constructor that doesn't require the SECTION
   * argument.
   */
  function AddTestCase(description, expect, actual) {
    new TestCase(SECTION, description, expect, actual);
  }
  global.AddTestCase = AddTestCase;

  function TestCase(n, d, e, a) {
    this.name = n;
    this.description = d;
    this.expect = e;
    this.actual = a;
    this.passed = getTestCaseResult(e, a);
    this.reason = '';
    this.bugnumber = typeof BUGNUMER !== 'undefined' ? BUGNUMBER : '';
    this.type = runningInShell ? 'shell' : 'browser';
    ObjectDefineProperty(
      gTestcases,
      gTc++,
      {
        __proto__: null,
        value: this,
        enumerable: true,
        configurable: true,
        writable: true
      }
    );
  }
  global.TestCase = TestCase;

  TestCase.prototype.dump = function () {
    // let reftest handle error reporting, otherwise
    // output a summary line.
    if (!isReftest) {
      dump('\njstest: ' + this.path + ' ' +
          'bug: '         + this.bugnumber + ' ' +
          'result: '      + (this.passed ? 'PASSED' : 'FAILED') + ' ' +
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

  function getTestCases() {
    return gTestcases;
  }
  global.getTestCases = getTestCases;

  /*
   * The test driver searches for such a phrase in the test output.
   * If such phrase exists, it will set n as the expected exit code.
   */
  function expectExitCode(n) {
    print('--- NOTE: IN THIS TESTCASE, WE EXPECT EXIT CODE ' + n + ' ---');
  }
  global.expectExitCode = expectExitCode;

  /*
   * Statuses current section of a test
   */
  function inSection(x) {
    return "Section " + x + " of test - ";
  }
  global.inSection = inSection;

  /*
   * Report a failure in the 'accepted' manner
   */
  function reportFailure(msg) {
    msg = String(msg);
    var lines = StringSplit(msg, "\n");
    var funcName = currentFunc();
    var prefix = funcName ? "[reported from " + funcName + "] ": "";

    for (var i = 0; i < lines.length; i++)
      print(FAILED + prefix + lines[i]);
  }
  global.reportFailure = reportFailure;

  /*
   * Print a non-failure message.
   */
  function printStatus(msg) {
    msg = String(msg);
    var lines = StringSplit(msg, "\n");

    for (var i = 0; i < lines.length; i++)
      print(STATUS + lines[i]);
  }
  global.printStatus = printStatus;

  /*
  * Print a bugnumber message.
  */
  function printBugNumber(num) {
    BUGNUMBER = num;
    print('BUGNUMBER: ' + num);
  }
  global.printBugNumber = printBugNumber;

  /*
   * Compare expected result to actual result, if they differ (in value and/or
   * type) report a failure.  If description is provided, include it in the
   * failure report.
   */
  function reportCompare(expected, actual, description) {
    var expected_t = typeof expected;
    var actual_t = typeof actual;
    var output = "";

    if (typeof description === "undefined")
      description = '';

    if (expected_t !== actual_t) {
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
    if (!isReftest) {
      if (testcase.passed) {
        print(PASSED + description);
      } else {
        reportFailure(description + " : " + output);
      }
    }
    return testcase.passed;
  }
  global.reportCompare = reportCompare;

  /*
   * Attempt to match a regular expression describing the result to
   * the actual result, if they differ (in value and/or
   * type) report a failure.  If description is provided, include it in the
   * failure report.
   */
  function reportMatch(expectedRegExp, actual, description) {
    var expected_t = "string";
    var actual_t = typeof actual;
    var output = "";

    if (typeof description === "undefined")
      description = '';

    if (expected_t !== actual_t) {
      output += "Type mismatch, expected type " + expected_t +
        ", actual type " + actual_t + " ";
    }

    var matches = ReflectApply(RegExpPrototypeExec, expectedRegExp, [actual]) !== null;
    if (!matches) {
      output += "Expected match to '" + toPrinted(expectedRegExp) +
        "', Actual value '" + toPrinted(actual) + "' ";
    }

    var testcase = new TestCase("unknown-test-name", description, true, matches);
    testcase.reason = output;

    // if running under reftest, let it handle result reporting.
    if (!isReftest) {
      if (testcase.passed) {
        print(PASSED + description);
      } else {
        reportFailure(description + " : " + output);
      }
    }
    return testcase.passed;
  }
  global.reportMatch = reportMatch;

  function normalizeSource(source) {
    source = String(source);
    source = StringReplace(source, /([(){},.:\[\]])/mg, (_, punctuator) => ` ${punctuator} `);
    source = StringReplace(source, /\s+/mg, _ => ' ');

    return source;
  }

  function compareSource(expect, actual, summary) {
    // compare source
    var expectP = normalizeSource(expect);
    var actualP = normalizeSource(actual);

    print('expect:\n' + expectP);
    print('actual:\n' + actualP);

    reportCompare(expectP, actualP, summary);

    // actual must be compilable if expect is?
    try {
      var expectCompile = 'No Error';
      var actualCompile;

      Function(expect);
      try {
        Function(actual);
        actualCompile = 'No Error';
      } catch(ex1) {
        actualCompile = ex1 + '';
      }
      reportCompare(expectCompile, actualCompile,
                    summary + ': compile actual');
    } catch(ex) {
    }
  }
  global.compareSource = compareSource;

  function getTestCaseResult(expected, actual) {
    if (typeof expected !== typeof actual)
      return false;
    if (typeof expected !== 'number')
      // Note that many tests depend on the use of '==' here, not '==='.
      return actual == expected;

    // Distinguish NaN from other values.  Using x !== x comparisons here
    // works even if tests redefine isNaN.
    if (actual !== actual)
      return expected !== expected;
    if (expected !== expected)
      return false;

    // Tolerate a certain degree of error.
    if (actual !== expected)
      return MathAbs(actual - expected) <= 1E-10;

    // Here would be a good place to distinguish 0 and -0, if we wanted
    // to.  However, doing so would introduce a number of failures in
    // areas where they don't seem important.  For example, the WeekDay
    // function in ECMA-262 returns -0 for Sundays before the epoch, but
    // the Date functions in SpiderMonkey specified in terms of WeekDay
    // often don't.  This seems unimportant.
    return true;
  }

  function test() {
    for (gTc = 0; gTc < gTestcases.length; gTc++) {
      // temporary hack to work around some unknown issue in 1.7
      try {
        var testCase = gTestcases[gTc];
        testCase.passed = writeTestCaseResult(
          testCase.expect,
          testCase.actual,
          testCase.description + " = " + testCase.actual);
        testCase.reason += testCase.passed ? "" : "wrong value ";
      } catch(e) {
        print('test(): empty testcase for gTc = ' + gTc + ' ' + e);
      }
    }
    return gTestcases;
  }
  global.test = test;

  /*
   * Begin printing functions.  These functions use the shell's
   * print function.  When running tests in the browser, browser.js
   * overrides these functions to write to the page.
   */
  function writeTestCaseResult(expect, actual, string) {
    var passed = getTestCaseResult(expect, actual);
    // if running under reftest, let it handle result reporting.
    if (!isReftest) {
      writeFormattedResult(expect, actual, string, passed);
    }
    return passed;
  }
  global.writeTestCaseResult = writeTestCaseResult;

  // Note: browser.js overrides this function.
  function writeHeaderToLog(string) {
    print(string);
  }
  global.writeHeaderToLog = writeHeaderToLog;
  /* end of print functions */

  // Note: browser.js overrides this function.
  function jsTestDriverEnd() {
    // gDelayTestDriverEnd is used to
    // delay collection of the test result and
    // signal to Spider so that tests can continue
    // to run after page load has fired. They are
    // responsible for setting gDelayTestDriverEnd = true
    // then when completed, setting gDelayTestDriverEnd = false
    // then calling jsTestDriverEnd()

    if (gDelayTestDriverEnd) {
      return;
    }

    try {
      optionsReset();
    } catch(ex) {
      dump('jsTestDriverEnd ' + ex);
    }

    for (var i = 0; i < gTestcases.length; i++) {
      gTestcases[i].dump();
    }
  }
  global.jsTestDriverEnd = jsTestDriverEnd;

  /************************************
   * PROMISE TESTING FUNCTION EXPORTS *
   ************************************/

  function getPromiseResult(promise) {
    var result, error, caught = false;
    promise.then(r => { result = r; },
                 e => { caught = true; error = e; });
    drainJobQueue();
    if (caught)
      throw error;
    return result;
  }

  function assertEventuallyEq(promise, expected) {
    assertEq(getPromiseResult(promise), expected);
  }
  global.assertEventuallyEq = assertEventuallyEq;

  function assertEventuallyThrows(promise, expectedErrorType) {
    assertThrowsInstanceOf(() => getPromiseResult(promise), expectedErrorType);
  };
  global.assertEventuallyThrows = assertEventuallyThrows;

  function assertEventuallyDeepEq(promise, expected) {
    assertDeepEq(getPromiseResult(promise), expected);
  };
  global.assertEventuallyDeepEq = assertEventuallyDeepEq;
})(this);

var STATUS = "STATUS: ";

var gDelayTestDriverEnd = false;
var gFailureExpected = false;

var gTestcases = new Array();
var gTc = gTestcases.length;

/*
 * constant strings
 */
var GLOBAL = this + '';

var DESCRIPTION;
var EXPECTED;

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
    if (optionName && optionName !== "ion") {
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

  for (var optionName in optionsframe)
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
