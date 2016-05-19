/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for " +
                 "bug 595934 - message categories coverage.";
const TESTS_PATH = "http://example.com/browser/devtools/client/webconsole/" +
                   "test/";
const TESTS = [
  {
    // #0
    file: "test-bug-595934-css-loader.html",
    category: "CSS Loader",
    matchString: "text/css",
  },
  {
    // #1
    file: "test-bug-595934-imagemap.html",
    category: "Layout: ImageMap",
    matchString: "shape=\"rect\"",
  },
  {
    // #2
    file: "test-bug-595934-html.html",
    category: "HTML",
    matchString: "multipart/form-data",
    onload: function () {
      let form = content.document.querySelector("form");
      form.submit();
    },
  },
  {
    // #3
    file: "test-bug-595934-workers.html",
    category: "Web Worker",
    matchString: "fooBarWorker",
  },
  {
    // #4
    file: "test-bug-595934-malformedxml.xhtml",
    category: "malformed-xml",
    matchString: "no element found",
  },
  {
    // #5
    file: "test-bug-595934-svg.xhtml",
    category: "SVG",
    matchString: "fooBarSVG",
  },
  {
    // #6
    file: "test-bug-595934-css-parser.html",
    category: "CSS Parser",
    matchString: "foobarCssParser",
  },
  {
    // #7
    file: "test-bug-595934-malformedxml-external.html",
    category: "malformed-xml",
    matchString: "</html>",
  },
  {
    // #8
    file: "test-bug-595934-empty-getelementbyid.html",
    category: "DOM",
    matchString: "getElementById",
  },
  {
    // #9
    file: "test-bug-595934-canvas-css.html",
    category: "CSS Parser",
    matchString: "foobarCanvasCssParser",
  },
  {
    // #10
    file: "test-bug-595934-image.html",
    category: "Image",
    matchString: "corrupt",
  },
];

var pos = -1;

var foundCategory = false;
var foundText = false;
var pageLoaded = false;
var pageError = false;
var output = null;
var jsterm = null;
var hud = null;
var testEnded = false;

var TestObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  observe: function testObserve(subject) {
    if (testEnded || !(subject instanceof Ci.nsIScriptError)) {
      return;
    }

    let expectedCategory = TESTS[pos].category;

    info("test #" + pos + " console observer got " + subject.category +
         ", is expecting " + expectedCategory);

    if (subject.category == expectedCategory) {
      foundCategory = true;
      startNextTest();
    } else {
      info("unexpected message was: " + subject.sourceName + ":" +
           subject.lineNumber + "; " + subject.errorMessage);
    }
  }
};

function consoleOpened(hudConsole) {
  hud = hudConsole;
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
    test = TESTS[pos];

    waitForMessages({
      webconsole: hud,
      messages: [{
        name: "message for test #" + pos + ": '" + test.matchString + "'",
        text: test.matchString,
      }],
    }).then(() => {
      foundText = true;
      startNextTest();
    });

    let testLocation = TESTS_PATH + test.file;
    gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
      if (content.location.href != testLocation) {
        return;
      }
      gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);

      pageLoaded = true;
      test.onload && test.onload(evt);

      if (test.expectError) {
        content.addEventListener("error", function _onError() {
          content.removeEventListener("error", _onError);
          pageError = true;
          startNextTest();
        });
        // On e10s, the exception is triggered in child process
        // and is ignored by test harness
        if (!Services.appinfo.browserTabsRemoteAutostart) {
          expectUncaughtException();
        }
      } else {
        pageError = true;
      }

      startNextTest();
    }, true);

    content.location = testLocation;
  } else {
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

  loadTab(TEST_URI).then(() => {
    openConsole().then(consoleOpened);
  });
}
