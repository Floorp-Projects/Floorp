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

  waitForSuccess({
    name: "20 console.log messages displayed",
    validatorFn: function()
    {
      return outputNode.textContent.indexOf("foo #29") > -1;
    },
    successFn: testNext,
    failureFn: finishTest,
  });

  yield;

  is(countMessageNodes(), 20, "there are 20 message nodes in the output " +
     "when the log limit is set to 20");

  console.log("bar bug585237");

  waitForSuccess({
    name: "another console.log message displayed",
    validatorFn: function()
    {
      return outputNode.textContent.indexOf("bar bug585237") > -1;
    },
    successFn: testNext,
    failureFn: finishTest,
  });

  yield;

  is(countMessageNodes(), 20, "there are still 20 message nodes in the " +
     "output when adding one more");

  prefBranch.setIntPref("console", 30);
  for (let i = 0; i < 20; i++) {
    console.log("boo #" + i); // must change message to prevent repeats
  }

  waitForSuccess({
    name: "another 20 console.log message displayed",
    validatorFn: function()
    {
      return outputNode.textContent.indexOf("boo #19") > -1;
    },
    successFn: testNext,
    failureFn: finishTest,
  });

  yield;

  is(countMessageNodes(), 30, "there are 30 message nodes in the output " +
     "when the log limit is set to 30");

  prefBranch.setIntPref("console", 0);
  console.log("baz");

  waitForSuccess({
    name: "clear output",
    validatorFn: function()
    {
      return countMessageNodes() == 0;
    },
    successFn: testNext,
    failureFn: finishTest,
  });

  yield;

  prefBranch.clearUserPref("console");
  hud = testDriver = prefBranch = console = outputNode = null;
  finishTest();

  yield;
}

function countMessageNodes() {
  return outputNode.querySelectorAll(".hud-msg-node").length;
}

