/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TESTS_PATH = "http://example.com/browser/browser/devtools/webconsole/test/";
const TESTS = [
  { // #0
    file: "test-bug-595934-css-loader.html",
    category: "CSS Loader",
    matchString: "text/css",
  },
  { // #1
    file: "test-bug-595934-dom-events.html",
    category: "DOM Events",
    matchString: "preventBubble()",
  },
  { // #2
    file: "test-bug-595934-dom-html.html",
    category: "DOM:HTML",
    matchString: "getElementById",
  },
  { // #3
    file: "test-bug-595934-imagemap.html",
    category: "ImageMap",
    matchString: "shape=\"rect\"",
  },
  { // #4
    file: "test-bug-595934-html.html",
    category: "HTML",
    matchString: "multipart/form-data",
    onload: function() {
      let form = content.document.querySelector("form");
      form.submit();
    },
  },
  { // #5
    file: "test-bug-595934-workers.html",
    category: "Web Worker",
    matchString: "fooBarWorker",
    expectError: true,
  },
  { // #6
    file: "test-bug-595934-malformedxml.xhtml",
    category: "malformed-xml",
    matchString: "no element found",
  },
  { // #7
    file: "test-bug-595934-svg.xhtml",
    category: "SVG",
    matchString: "fooBarSVG",
  },
  { // #8
    file: "test-bug-595934-dom-html-external.html",
    category: "DOM:HTML",
    matchString: "document.all",
  },
  { // #9
    file: "test-bug-595934-dom-events-external2.html",
    category: "DOM Events",
    matchString: "preventBubble()",
  },
  { // #10
    file: "test-bug-595934-canvas.html",
    category: "Canvas",
    matchString: "strokeStyle",
  },
  { // #11
    file: "test-bug-595934-css-parser.html",
    category: "CSS Parser",
    matchString: "foobarCssParser",
  },
  { // #12
    file: "test-bug-595934-malformedxml-external.html",
    category: "malformed-xml",
    matchString: "</html>",
  },
  { // #14
    file: "test-bug-595934-empty-getelementbyid.html",
    category: "DOM",
    matchString: "getElementById",
  },
  { // #15
    file: "test-bug-595934-canvas-css.html",
    category: "CSS Parser",
    matchString: "foobarCanvasCssParser",
  },
  { // #16
    file: "test-bug-595934-image.html",
    category: "Image",
    matchString: "corrupt",
  },
];

let pos = -1;

let foundCategory = false;
let foundText = false;
let pageLoaded = false;
let pageError = false;
let output = null;
let jsterm = null;
let testEnded = false;

let TestObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  observe: function test_observe(aSubject)
  {
    if (testEnded || !(aSubject instanceof Ci.nsIScriptError)) {
      return;
    }

    is(aSubject.category, TESTS[pos].category,
      "test #" + pos + ": error category '" + TESTS[pos].category + "'");

    if (aSubject.category == TESTS[pos].category) {
      foundCategory = true;
    }
    else {
      ok(false, aSubject.sourceName + ':' + aSubject.lineNumber + '; ' +
                aSubject.errorMessage);
      testEnded = true;
    }
  }
};

function consoleOpened(hud) {
  output = hud.outputNode;
  output.addEventListener("DOMNodeInserted", onDOMNodeInserted, false);
  jsterm = hud.jsterm;

  Services.console.registerListener(TestObserver);

  registerCleanupFunction(testEnd);

  executeSoon(testNext);
}

function testNext() {
  jsterm.clearOutput();
  foundCategory = false;
  foundText = false;
  pageLoaded = false;
  pageError = false;

  pos++;
  if (pos < TESTS.length) {
    waitForSuccess({
      name: "test #" + pos + " succesful finish",
      validatorFn: function()
      {
        return foundCategory && foundText && pageLoaded && pageError;
      },
      successFn: testNext,
      failureFn: function() {
        info("foundCategory " + foundCategory + " foundText " + foundText +
             " pageLoaded " + pageLoaded + " pageError " + pageError);
        finishTest();
      },
    });

    let test = TESTS[pos];
    let testLocation = TESTS_PATH + test.file;
    browser.addEventListener("load", function onLoad(aEvent) {
      if (content.location.href != testLocation) {
        return;
      }
      browser.removeEventListener(aEvent.type, onLoad, true);

      pageLoaded = true;
      test.onload && test.onload(aEvent);

      if (test.expectError) {
        content.addEventListener("error", function _onError() {
          content.removeEventListener("error", _onError);
          pageError = true;
        });
        expectUncaughtException();
      }
      else {
        pageError = true;
      }
    }, true);

    content.location = testLocation;
  }
  else {
    testEnded = true;
    executeSoon(finishTest);
  }
}

function testEnd() {
  Services.console.unregisterListener(TestObserver);
  output.removeEventListener("DOMNodeInserted", onDOMNodeInserted, false);
  TestObserver = output = jsterm = null;
}

function onDOMNodeInserted(aEvent) {
  let textContent = output.textContent;
  foundText = textContent.indexOf(TESTS[pos].matchString) > -1;
  if (foundText) {
    ok(foundText, "test #" + pos + ": message found '" + TESTS[pos].matchString + "'");
  }
}

function test() {
  addTab("data:text/html;charset=utf-8,Web Console test for bug 595934 - message categories coverage.");
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

