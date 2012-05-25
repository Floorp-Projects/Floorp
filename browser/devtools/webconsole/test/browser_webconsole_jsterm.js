/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

let jsterm, testDriver;

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, function(hud) {
      testDriver = testJSTerm(hud);
      testDriver.next();
    });
  }, true);
}

function nextTest() {
  testDriver.next();
}

function checkResult(msg, desc, lines) {
  waitForSuccess({
    name: "correct number of results shown for " + desc,
    validatorFn: function()
    {
      let nodes = jsterm.outputNode.querySelectorAll(".webconsole-msg-output");
      return nodes.length == lines;
    },
    successFn: function()
    {
      let labels = jsterm.outputNode.querySelectorAll(".webconsole-msg-output");
      if (typeof msg == "string") {
        is(labels[lines-1].textContent.trim(), msg,
           "correct message shown for " + desc);
      }
      else if (typeof msg == "function") {
        ok(msg(labels), "correct message shown for " + desc);
      }

      nextTest();
    },
    failureFn: nextTest,
  });
}

function testJSTerm(hud)
{
  jsterm = hud.jsterm;

  jsterm.clearOutput();
  jsterm.execute("'id=' + $('header').getAttribute('id')");
  checkResult('"id=header"', "$() worked", 1);
  yield;

  jsterm.clearOutput();
  jsterm.execute("headerQuery = $$('h1')");
  jsterm.execute("'length=' + headerQuery.length");
  checkResult('"length=1"', "$$() worked", 2);
  yield;

  jsterm.clearOutput();
  jsterm.execute("xpathQuery = $x('.//*', document.body);");
  jsterm.execute("'headerFound='  + (xpathQuery[0] == headerQuery[0])");
  checkResult('"headerFound=true"', "$x() worked", 2);
  yield;

  // no jsterm.clearOutput() here as we clear the output using the clear() fn.
  jsterm.execute("clear()");

  waitForSuccess({
    name: "clear() worked",
    validatorFn: function()
    {
      return jsterm.outputNode.childNodes.length == 0;
    },
    successFn: nextTest,
    failureFn: nextTest,
  });

  yield;

  jsterm.clearOutput();
  jsterm.execute("'keysResult=' + (keys({b:1})[0] == 'b')");
  checkResult('"keysResult=true"', "keys() worked", 1);
  yield;

  jsterm.clearOutput();
  jsterm.execute("'valuesResult=' + (values({b:1})[0] == 1)");
  checkResult('"valuesResult=true"', "values() worked", 1);
  yield;

  jsterm.clearOutput();

  let tabs = gBrowser.tabs.length;

  jsterm.execute("help()");
  let output = jsterm.outputNode.querySelector(".webconsole-msg-output");
  ok(!output, "help() worked");

  jsterm.execute("help");
  output = jsterm.outputNode.querySelector(".webconsole-msg-output");
  ok(!output, "help worked");

  jsterm.execute("?");
  output = jsterm.outputNode.querySelector(".webconsole-msg-output");
  ok(!output, "? worked");

  let foundTab = null;
  waitForSuccess({
    name: "help tab opened",
    validatorFn: function()
    {
      let newTabOpen = gBrowser.tabs.length == tabs + 1;
      if (!newTabOpen) {
        return false;
      }

      foundTab = gBrowser.tabs[tabs];
      return true;
    },
    successFn: function()
    {
      gBrowser.removeTab(foundTab);
      nextTest();
    },
    failureFn: nextTest,
  });
  yield;

  jsterm.clearOutput();
  jsterm.execute("pprint({b:2, a:1})");
  checkResult("a: 1\n  b: 2", "pprint()", 1);
  yield;

  // check instanceof correctness, bug 599940
  jsterm.clearOutput();
  jsterm.execute("[] instanceof Array");
  checkResult("true", "[] instanceof Array == true", 1);
  yield;

  jsterm.clearOutput();
  jsterm.execute("({}) instanceof Object");
  checkResult("true", "({}) instanceof Object == true", 1);
  yield;

  // check for occurrences of Object XRayWrapper, bug 604430
  jsterm.clearOutput();
  jsterm.execute("document");
  checkResult(function(nodes) {
    return nodes[0].textContent.search(/\[object xraywrapper/i) == -1;
  }, "document - no XrayWrapper", 1);
  yield;

  // check that pprint(window) and keys(window) don't throw, bug 608358
  jsterm.clearOutput();
  jsterm.execute("pprint(window)");
  checkResult(null, "pprint(window)", 1);
  yield;

  jsterm.clearOutput();
  jsterm.execute("keys(window)");
  checkResult(null, "keys(window)", 1);
  yield;

  // bug 614561
  jsterm.clearOutput();
  jsterm.execute("pprint('hi')");
  checkResult('0: "h"\n  1: "i"', "pprint('hi')", 1);
  yield;

  // check that pprint(function) shows function source, bug 618344
  jsterm.clearOutput();
  jsterm.execute("pprint(print)");
  checkResult(function(nodes) {
    return nodes[0].textContent.indexOf("aJSTerm.") > -1;
  }, "pprint(function) shows source", 1);
  yield;

  // check that an evaluated null produces "null", bug 650780
  jsterm.clearOutput();
  jsterm.execute("null");
  checkResult("null", "null is null", 1);
  yield;

  jsterm = testDriver = null;
  executeSoon(finishTest);
  yield;
}
