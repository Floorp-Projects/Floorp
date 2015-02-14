/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 1112378
 * Tests to ensure that errors called on wrapped functions via call-watcher
 * correctly looks like the error comes from the content, not from within the devtools.
 */

const BUG_1112378_URL = EXAMPLE_URL + "doc_bug_1112378.html";

add_task(function*() {
  let { target, panel } = yield initWebAudioEditor(BUG_1112378_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS, gAudioNodes } = panelWin;

  loadFrameScripts();

  reload(target);

  yield waitForGraphRendered(panelWin, 2, 0);

  let error = yield evalInDebuggee("throwError()");
  is(error.lineNumber, 21, "error has correct lineNumber");
  is(error.columnNumber, 11, "error has correct columnNumber");
  is(error.name, "TypeError", "error has correct name");
  is(error.message, "Argument 1 is not valid for any of the 2-argument overloads of AudioNode.connect.", "error has correct message");
  is(error.stringified, "TypeError: Argument 1 is not valid for any of the 2-argument overloads of AudioNode.connect.", "error is stringified correctly");
  ise(error.instanceof, true, "error is correctly an instanceof TypeError");
  is(error.fileName, "http://example.com/browser/browser/devtools/webaudioeditor/test/doc_bug_1112378.html", "error has correct fileName");

  error = yield evalInDebuggee("throwDOMException()");
  is(error.lineNumber, 37, "exception has correct lineNumber");
  is(error.columnNumber, 0, "exception has correct columnNumber");
  is(error.code, 9, "exception has correct code");
  is(error.result, 2152923145, "exception has correct result");
  is(error.name, "NotSupportedError", "exception has correct name");
  is(error.message, "Operation is not supported", "exception has correct message");
  is(error.stringified, "NotSupportedError: Operation is not supported", "exception is stringified correctly");
  ise(error.instanceof, true, "exception is correctly an instance of DOMException");
  is(error.filename, "http://example.com/browser/browser/devtools/webaudioeditor/test/doc_bug_1112378.html", "exception has correct filename");

  yield teardown(target);
});
