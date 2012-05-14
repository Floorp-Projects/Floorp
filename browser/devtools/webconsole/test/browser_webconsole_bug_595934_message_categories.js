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
    file: "test-bug-595934-malformedxml.xhtml",
    category: "malformed-xml",
    matchString: "no element found",
  },
  { // #6
    file: "test-bug-595934-svg.xhtml",
    category: "SVG",
    matchString: "fooBarSVG",
  },
  { // #7
    file: "test-bug-595934-dom-html-external.html",
    category: "DOM:HTML",
    matchString: "document.all",
  },
  { // #8
    file: "test-bug-595934-dom-events-external2.html",
    category: "DOM Events",
    matchString: "preventBubble()",
  },
  { // #9
    file: "test-bug-595934-canvas.html",
    category: "Canvas",
    matchString: "strokeStyle",
  },
  { // #10
    file: "test-bug-595934-css-parser.html",
    category: "CSS Parser",
    matchString: "foobarCssParser",
  },
  { // #11
    file: "test-bug-595934-malformedxml-external.html",
    category: "malformed-xml",
    matchString: "</html>",
  },
  { // #12
    file: "test-bug-595934-empty-getelementbyid.html",
    category: "DOM",
    matchString: "getElementById",
  },
  { // #13
    file: "test-bug-595934-canvas-css.html",
    category: "CSS Parser",
    matchString: "foobarCanvasCssParser",
  },
  { // #14
    file: "test-bug-595934-image.html",
    category: "Image",
    matchString: "corrupt",
  },
  { // #15
    file: "test-bug-595934-workers.html",
    category: "Web Worker",
    matchString: "fooBarWorker",
    expectError: true,
  },
];

let pos = -1;

let foundCategory = false;
let foundText = false;
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
      if (foundText) {
        executeSoon(testNext);
      }
    }
    else {
      ok(false, aSubject.sourceName + ':' + aSubject.lineNumber + '; ' +
                aSubject.errorMessage);
      testEnded = true;
      executeSoon(finishTest);
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

  pos++;
  if (pos < TESTS.length) {
    let test = TESTS[pos];
    let testLocation = TESTS_PATH + test.file;
    if (test.onload) {
      browser.addEventListener("load", function onLoad(aEvent) {
        if (content.location.href == testLocation) {
          browser.removeEventListener(aEvent.type, onLoad, true);
          test.onload(aEvent);
        }
      }, true);
    }

    if (test.expectError) {
      expectUncaughtException();
    }

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

  if (foundCategory) {
    executeSoon(testNext);
  }
}

function test() {
  addTab("data:text/html;charset=utf-8,Web Console test for bug 595934 - message categories coverage.");
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

