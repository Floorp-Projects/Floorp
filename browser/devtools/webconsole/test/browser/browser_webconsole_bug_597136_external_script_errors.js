/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Patrick Walton <pcwalton@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/" +
                 "webconsole/test/browser/test-bug-597136-external-script-" +
                 "errors.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoaded, true);
}

function tabLoaded(aEvent) {
  browser.removeEventListener("load", tabLoaded, true);
  openConsole();

  browser.addEventListener("load", contentLoaded, true);
  content.location.reload();
}

function contentLoaded(aEvent) {
  browser.removeEventListener("load", contentLoaded, true);

  let button = content.document.querySelector("button");
  expectUncaughtException();
  EventUtils.sendMouseEvent({ type: "click" }, button, content);
  executeSoon(buttonClicked);
}

function buttonClicked() {
  let outputNode = HUDService.getHudByWindow(content).outputNode;

  let msg = "the error from the external script was logged";
  testLogEntry(outputNode, "bogus", msg);

  finishTest();
}

