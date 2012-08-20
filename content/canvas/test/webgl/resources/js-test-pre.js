/*
Copyright (C) 2009 Apple Computer, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// WebKit Specfic code.  Add your own here.
function initNonKhronosFramework(waitUntilDone) {
  if (window.layoutTestController) {
    layoutTestController.overridePreference("WebKitWebGLEnabled", "1");
    layoutTestController.dumpAsText();
    if (waitUntilDone) {
      layoutTestController.waitUntilDone();
    }
  }
}

function nonKhronosFrameworkNotifyDone() {
  if (window.layoutTestController) {
    layoutTestController.notifyDone();
  }
}

function reportTestResultsToHarness(success, msg) {
  if (window.parent.webglTestHarness) {
    window.parent.webglTestHarness.reportResults(success, msg);
  }
}

function notifyFinishedToHarness() {
  if (window.parent.webglTestHarness) {
    window.parent.webglTestHarness.notifyFinished();
  }
}

function description(msg)
{
    // For MSIE 6 compatibility
    var span = document.createElement("span");
    span.innerHTML = '<p>' + msg + '</p><p>On success, you will see a series of "<span class="pass">PASS</span>" messages, followed by "<span class="pass">TEST COMPLETE</span>".</p>';
    var description = document.getElementById("description");
    if (description.firstChild)
        description.replaceChild(span, description.firstChild);
    else
        description.appendChild(span);
}

function debug(msg)
{
    var span = document.createElement("span");
    document.getElementById("console").appendChild(span); // insert it first so XHTML knows the namespace
    span.innerHTML = msg + '<br />';
}

function escapeHTML(text)
{
    return text.replace(/&/g, "&amp;").replace(/</g, "&lt;");
}

function testPassed(msg)
{
    reportTestResultsToHarness(true, msg);
    debug('<span><span class="pass">PASS</span> ' + escapeHTML(msg) + '</span>');
}

function testFailed(msg)
{
    reportTestResultsToHarness(false, msg);
    debug('<span><span class="fail">FAIL</span> ' + escapeHTML(msg) + '</span>');
    dump('FAIL: ' + msg + '\n');

    var stack = (new Error).stack.split('\n');
    if (!stack.length) {
        return;
    }

    dump('STACK TRACE: \n');

    stack.pop();
    var index = 0, frame, messages = new Array();
    // Match all .html files and print out the line in them.
    while (stack.length && index != -1) {
        frame = stack.pop();
        index = frame.indexOf(".html:");
        if (index != -1) {
            messages.unshift(frame);
        }
    }

    // Print out the first stack frame in JS and then stop.
    if (stack.length) {
        messages.unshift(stack.pop());
    }

    for (message in messages) {
        dump(messages[message] + '\n');
    }
}

function testFailedRender(msg, ref, test, width, height) 
{
    var refData;
    if (typeof ref.getImageData == 'function') {
        refData = ref.canvas.toDataURL();
    } else {
        refData = arrayToURLData(ref, width, height);
    }

    var testData;
    if (typeof test.getImageData == 'function') {
        testData = test.canvas.toDataURL();
    } else {
        testData = arrayToURLData(test, width, height);
    }
    
    testFailed(msg);

    var data = 'REFTEST TEST-DEBUG-INFO | ' + msg + ' | image comparison (==)\n' +
               'REFTEST   IMAGE 1 (TEST): ' + testData + '\n' +
               'REFTEST   IMAGE 2 (REFERENCE): ' + refData;
    dump('FAIL: ' + data + '\n');
    dump('To view the differences between these image renderings, go to the following link: https://hg.mozilla.org/mozilla-central/raw-file/tip/layout/tools/reftest/reftest-analyzer.xhtml#log=' +
    encodeURIComponent(encodeURIComponent(data)) + '\n');
}

function arrayToURLData(buf, width, height)
{
    var cv = document.createElement('canvas');
    cv.height = height;
    cv.width = width;
    var ctx = cv.getContext('2d');
    var imgd = ctx.getImageData(0, 0, width, height);
    for (i = 0; i < height * width; ++i) {
        offset = i * 4;
        for (j = 0; j < 4; j++) {
            imgd.data[offset + j] = buf[offset + j];
        }
    }
    ctx.putImageData(imgd, 0, 0);
    return cv.toDataURL();
}

function areArraysEqual(_a, _b)
{
    try {
        if (_a.length !== _b.length)
            return false;
        for (var i = 0; i < _a.length; i++)
            if (_a[i] !== _b[i])
                return false;
    } catch (ex) {
        return false;
    }
    return true;
}

function isMinusZero(n)
{
    // the only way to tell 0 from -0 in JS is the fact that 1/-0 is
    // -Infinity instead of Infinity
    return n === 0 && 1/n < 0;
}

function isResultCorrect(_actual, _expected)
{
    if (_expected === 0)
        return _actual === _expected && (1/_actual) === (1/_expected);
    if (_actual === _expected)
        return true;
    if (typeof(_expected) == "number" && isNaN(_expected))
        return typeof(_actual) == "number" && isNaN(_actual);
    if (Object.prototype.toString.call(_expected) == Object.prototype.toString.call([]))
        return areArraysEqual(_actual, _expected);
    return false;
}

function stringify(v)
{
    if (v === 0 && 1/v < 0)
        return "-0";
    else return "" + v;
}

function evalAndLog(_a)
{
  if (typeof _a != "string")
    debug("WARN: tryAndLog() expects a string argument");

  // Log first in case things go horribly wrong or this causes a sync event.
  debug(_a);

  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
    testFailed(_a + " threw exception " + e);
  }
  return _av;
}

function shouldBe(_a, _b, quiet)
{
    if (typeof _a != "string" || typeof _b != "string")
        debug("WARN: shouldBe() expects string arguments");
    var exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        exception = e;
    }
    var _bv = eval(_b);

    if (exception)
        testFailed(_a + " should be " + _bv + ". Threw exception " + exception);
    else if (isResultCorrect(_av, _bv)) {
        if (!quiet) {
            testPassed(_a + " is " + _b);
        }
    } else if (typeof(_av) == typeof(_bv))
        testFailed(_a + " should be " + _bv + ". Was " + stringify(_av) + ".");
    else
        testFailed(_a + " should be " + _bv + " (of type " + typeof _bv + "). Was " + _av + " (of type " + typeof _av + ").");
}

function shouldNotBe(_a, _b, quiet)
{
    if (typeof _a != "string" || typeof _b != "string")
        debug("WARN: shouldNotBe() expects string arguments");
    var exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        exception = e;
    }
    var _bv = eval(_b);

    if (exception)
        testFailed(_a + " should not be " + _bv + ". Threw exception " + exception);
    else if (!isResultCorrect(_av, _bv)) {
        if (!quiet) {
            testPassed(_a + " is not " + _b);
        }
    } else
        testFailed(_a + " should not be " + _bv + ".");
}

function shouldBeTrue(_a) { shouldBe(_a, "true"); }
function shouldBeFalse(_a) { shouldBe(_a, "false"); }
function shouldBeNaN(_a) { shouldBe(_a, "NaN"); }
function shouldBeNull(_a) { shouldBe(_a, "null"); }

function shouldBeEqualToString(a, b)
{
  var unevaledString = '"' + b.replace(/"/g, "\"") + '"';
  shouldBe(a, unevaledString);
}

function shouldEvaluateTo(actual, expected) {
  // A general-purpose comparator.  'actual' should be a string to be
  // evaluated, as for shouldBe(). 'expected' may be any type and will be
  // used without being eval'ed.
  if (expected == null) {
    // Do this before the object test, since null is of type 'object'.
    shouldBeNull(actual);
  } else if (typeof expected == "undefined") {
    shouldBeUndefined(actual);
  } else if (typeof expected == "function") {
    // All this fuss is to avoid the string-arg warning from shouldBe().
    try {
      actualValue = eval(actual);
    } catch (e) {
      testFailed("Evaluating " + actual + ": Threw exception " + e);
      return;
    }
    shouldBe("'" + actualValue.toString().replace(/\n/g, "") + "'",
             "'" + expected.toString().replace(/\n/g, "") + "'");
  } else if (typeof expected == "object") {
    shouldBeTrue(actual + " == '" + expected + "'");
  } else if (typeof expected == "string") {
    shouldBe(actual, expected);
  } else if (typeof expected == "boolean") {
    shouldBe("typeof " + actual, "'boolean'");
    if (expected)
      shouldBeTrue(actual);
    else
      shouldBeFalse(actual);
  } else if (typeof expected == "number") {
    shouldBe(actual, stringify(expected));
  } else {
    debug(expected + " is unknown type " + typeof expected);
    shouldBeTrue(actual, "'"  +expected.toString() + "'");
  }
}

function shouldBeNonZero(_a)
{
  var exception;
  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
     exception = e;
  }

  if (exception)
    testFailed(_a + " should be non-zero. Threw exception " + exception);
  else if (_av != 0)
    testPassed(_a + " is non-zero.");
  else
    testFailed(_a + " should be non-zero. Was " + _av);
}

function shouldBeNonNull(_a)
{
  var exception;
  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
     exception = e;
  }

  if (exception)
    testFailed(_a + " should be non-null. Threw exception " + exception);
  else if (_av != null)
    testPassed(_a + " is non-null.");
  else
    testFailed(_a + " should be non-null. Was " + _av);
}

function shouldBeUndefined(_a)
{
  var exception;
  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
     exception = e;
  }

  if (exception)
    testFailed(_a + " should be undefined. Threw exception " + exception);
  else if (typeof _av == "undefined")
    testPassed(_a + " is undefined.");
  else
    testFailed(_a + " should be undefined. Was " + _av);
}

function shouldBeDefined(_a)
{
  var exception;
  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
     exception = e;
  }

  if (exception)
    testFailed(_a + " should be defined. Threw exception " + exception);
  else if (_av !== undefined)
    testPassed(_a + " is defined.");
  else
    testFailed(_a + " should be defined. Was " + _av);
}

function shouldBeGreaterThanOrEqual(_a, _b) {
    if (typeof _a != "string" || typeof _b != "string")
        debug("WARN: shouldBeGreaterThanOrEqual expects string arguments");

    var exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        exception = e;
    }
    var _bv = eval(_b);

    if (exception)
        testFailed(_a + " should be >= " + _b + ". Threw exception " + exception);
    else if (typeof _av == "undefined" || _av < _bv)
        testFailed(_a + " should be >= " + _b + ". Was " + _av + " (of type " + typeof _av + ").");
    else
        testPassed(_a + " is >= " + _b);
}

function shouldThrow(_a, _e)
{
  var exception;
  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
     exception = e;
  }

  var _ev;
  if (_e)
      _ev =  eval(_e);

  if (exception) {
    if (typeof _e == "undefined" || exception == _ev)
      testPassed(_a + " threw exception " + exception + ".");
    else
      testFailed(_a + " should throw " + (typeof _e == "undefined" ? "an exception" : _ev) + ". Threw exception " + exception + ".");
  } else if (typeof _av == "undefined")
    testFailed(_a + " should throw " + (typeof _e == "undefined" ? "an exception" : _ev) + ". Was undefined.");
  else
    testFailed(_a + " should throw " + (typeof _e == "undefined" ? "an exception" : _ev) + ". Was " + _av + ".");
}

function assertMsg(assertion, msg) {
    if (assertion) {
        testPassed(msg);
    } else {
        testFailed(msg);
    }
}

function gc() {
    if (window.GCController) {
        window.GCController.collect();
        return;
    }

    if (window.opera && window.opera.collect) {
        window.opera.collect();
        return;
    }

    try {
        window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
              .getInterface(Components.interfaces.nsIDOMWindowUtils)
              .garbageCollect();
        return;
    } catch(e) {}

    function gcRec(n) {
        if (n < 1)
            return {};
        var temp = {i: "ab" + i + (i / 100000)};
        temp += "foo";
        gcRec(n-1);
    }
    for (var i = 0; i < 1000; i++)
        gcRec(10);
}

function finishTest() {
    debug('<br /><span class="pass">TEST COMPLETE</span>');
    notifyFinishedToHarness();
}

