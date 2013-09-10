/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Patrick Walton <pcwalton@mozilla.com>
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that the Web Console limits the number of lines displayed according to
// the user's preferences.

const TEST_URI = "data:text/html;charset=utf8,test for bug 585237";
let hud, testDriver;

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, function(aHud) {
      hud = aHud;
      testDriver = testGen();
      testNext();
    });
  }, true);
}

function testNext() {
  testDriver.next();
}

function testGen() {
  let console = content.console;
  outputNode = hud.outputNode;

  hud.jsterm.clearOutput();

  let prefBranch = Services.prefs.getBranch("devtools.hud.loglimit.");
  prefBranch.setIntPref("console", 20);

  for (let i = 0; i < 30; i++) {
    console.log("foo #" + i); // must change message to prevent repeats
  }

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foo #29",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(testNext);

  yield undefined;

  is(countMessageNodes(), 20, "there are 20 message nodes in the output " +
     "when the log limit is set to 20");

  console.log("bar bug585237");

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "bar bug585237",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(testNext);

  yield undefined;

  is(countMessageNodes(), 20, "there are still 20 message nodes in the " +
     "output when adding one more");

  prefBranch.setIntPref("console", 30);
  for (let i = 0; i < 20; i++) {
    console.log("boo #" + i); // must change message to prevent repeats
  }

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "boo #19",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(testNext);

  yield undefined;

  is(countMessageNodes(), 30, "there are 30 message nodes in the output " +
     "when the log limit is set to 30");

  prefBranch.clearUserPref("console");
  hud = testDriver = prefBranch = console = outputNode = null;
  finishTest();

  yield undefined;
}

function countMessageNodes() {
  return outputNode.querySelectorAll(".message").length;
}

