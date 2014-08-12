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
    file: "test-bug-595934-imagemap.html",
    category: "Layout: ImageMap",
    matchString: "shape=\"rect\"",
  },
  { // #2
    file: "test-bug-595934-html.html",
    category: "HTML",
    matchString: "multipart/form-data",
    onload: function() {
      let form = content.document.querySelector("form");
      form.submit();
    },
  },
  { // #3
    file: "test-bug-595934-workers.html",
    category: "Web Worker",
    matchString: "fooBarWorker",
    expectError: true,
  },
  { // #4
    file: "test-bug-595934-malformedxml.xhtml",
    category: "malformed-xml",
    matchString: "no element found",
  },
  { // #5
    file: "test-bug-595934-svg.xhtml",
    category: "SVG",
    matchString: "fooBarSVG",
  },
  { // #6
    file: "test-bug-595934-css-parser.html",
    category: "CSS Parser",
    matchString: "foobarCssParser",
  },
  { // #7
    file: "test-bug-595934-malformedxml-external.html",
    category: "malformed-xml",
    matchString: "</html>",
  },
  { // #8
    file: "test-bug-595934-empty-getelementbyid.html",
    category: "DOM",
    matchString: "getElementById",
  },
  { // #9
    file: "test-bug-595934-canvas-css.html",
    category: "CSS Parser",
    matchString: "foobarCanvasCssParser",
  },
  { // #10
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
let hud = null;
let testEnded = false;

let TestObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  observe: function test_observe(aSubject)
  {
    if (testEnded || !(aSubject instanceof Ci.nsIScriptError)) {
      return;
    }

    var expectedCategory = TESTS[pos].category;

    info("test #" + pos + " console observer got " + aSubject.category +
         ", is expecting " + expectedCategory);

    if (aSubject.category == expectedCategory) {
      foundCategory = true;
      startNextTest();
    }
    else {
      info("unexpected message was: " + aSubject.sourceName + ":" +
           aSubject.lineNumber + "; " + aSubject.errorMessage);
    }
  }
};

function consoleOpened(aHud) {
  hud = aHud;
  output = hud.outputNode;
  jsterm = hud.jsterm;

  Services.console.registerListener(TestObserver);

  registerCleanupFunction(testEnd);

  testNext();
}

function testNext() {
  jsterm.clearOutput();
  foundCategory = false;
  foundText = false;
  pageLoaded = false;
  pageError = false;

  pos++;
  info("testNext: #" + pos);
  if (pos < TESTS.length) {
    let test = TESTS[pos];

    waitForMessages({
      webconsole: hud,
      messages: [{
        name: "message for test #" + pos + ": '" + test.matchString +"'",
        text: test.matchString,
      }],
    }).then(() => {
      foundText = true;
      startNextTest();
    });

    let testLocation = TESTS_PATH + test.file;
    gBrowser.selectedBrowser.addEventListener("load", function onLoad(aEvent) {
      if (content.location.href != testLocation) {
        return;
      }
      gBrowser.selectedBrowser.removeEventListener(aEvent.type, onLoad, true);

      pageLoaded = true;
      test.onload && test.onload(aEvent);

      if (test.expectError) {
        content.addEventListener("error", function _onError() {
          content.removeEventListener("error", _onError);
          pageError = true;
          startNextTest();
        });
        expectUncaughtException();
      }
      else {
        pageError = true;
      }

      startNextTest();
    }, true);

    content.location = testLocation;
  }
  else {
    testEnded = true;
    finishTest();
  }
}

function testEnd() {
  if (!testEnded) {
    info("foundCategory " + foundCategory + " foundText " + foundText +
         " pageLoaded " + pageLoaded + " pageError " + pageError);
  }

  Services.console.unregisterListener(TestObserver);
  hud = TestObserver = output = jsterm = null;
}

function startNextTest() {
  if (!testEnded && foundCategory && foundText && pageLoaded && pageError) {
    testNext();
  }
}

function test() {
  requestLongerTimeout(2);

  addTab("data:text/html;charset=utf-8,Web Console test for bug 595934 - message categories coverage.");
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

