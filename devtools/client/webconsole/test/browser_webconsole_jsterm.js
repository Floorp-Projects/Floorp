/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

var jsterm;

var test = asyncTest(function* () {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  jsterm = hud.jsterm;
  yield testJSTerm(hud);
  jsterm = null;
});

function checkResult(msg, desc) {
  let def = promise.defer();
  waitForMessages({
    webconsole: jsterm.hud.owner,
    messages: [{
      name: desc,
      category: CATEGORY_OUTPUT,
    }],
  }).then(([result]) => {
    let node = [...result.matched][0].querySelector(".message-body");
    if (typeof msg == "string") {
      is(node.textContent.trim(), msg,
        "correct message shown for " + desc);
    } else if (typeof msg == "function") {
      ok(msg(node), "correct message shown for " + desc);
    }

    def.resolve();
  });
  return def.promise;
}

function* testJSTerm(hud) {
  const HELP_URL = "https://developer.mozilla.org/docs/Tools/" +
                   "Web_Console/Helpers";

  jsterm.clearOutput();
  yield jsterm.execute("$('#header').getAttribute('id')");
  yield checkResult('"header"', "$() worked");

  jsterm.clearOutput();
  yield jsterm.execute("$$('h1').length");
  yield checkResult("1", "$$() worked");

  jsterm.clearOutput();
  yield jsterm.execute("$x('.//*', document.body)[0] == $$('h1')[0]");
  yield checkResult("true", "$x() worked");

  // no jsterm.clearOutput() here as we clear the output using the clear() fn.
  yield jsterm.execute("clear()");

  yield waitForSuccess({
    name: "clear() worked",
    validator: function() {
      return jsterm.outputNode.childNodes.length == 0;
    }
  });

  jsterm.clearOutput();
  yield jsterm.execute("keys({b:1})[0] == 'b'");
  yield checkResult("true", "keys() worked", 1);

  jsterm.clearOutput();
  yield jsterm.execute("values({b:1})[0] == 1");
  yield checkResult("true", "values() worked", 1);

  jsterm.clearOutput();

  let openedLinks = 0;
  let oldOpenLink = hud.openLink;
  hud.openLink = (url) => {
    if (url == HELP_URL) {
      openedLinks++;
    }
  };

  yield jsterm.execute("help()");
  yield jsterm.execute("help");
  yield jsterm.execute("?");

  let output = jsterm.outputNode.querySelector(".message[category='output']");
  ok(!output, "no output for help() calls");
  is(openedLinks, 3, "correct number of pages opened by the help calls");
  hud.openLink = oldOpenLink;

  jsterm.clearOutput();
  yield jsterm.execute("pprint({b:2, a:1})");
  yield checkResult("\"  b: 2\n  a: 1\"", "pprint()");

  // check instanceof correctness, bug 599940
  jsterm.clearOutput();
  yield jsterm.execute("[] instanceof Array");
  yield checkResult("true", "[] instanceof Array == true");

  jsterm.clearOutput();
  yield jsterm.execute("({}) instanceof Object");
  yield checkResult("true", "({}) instanceof Object == true");

  // check for occurrences of Object XRayWrapper, bug 604430
  jsterm.clearOutput();
  yield jsterm.execute("document");
  yield checkResult(function(node) {
    return node.textContent.search(/\[object xraywrapper/i) == -1;
  }, "document - no XrayWrapper");

  // check that pprint(window) and keys(window) don't throw, bug 608358
  jsterm.clearOutput();
  yield jsterm.execute("pprint(window)");
  yield checkResult(null, "pprint(window)");

  jsterm.clearOutput();
  yield jsterm.execute("keys(window)");
  yield checkResult(null, "keys(window)");

  // bug 614561
  jsterm.clearOutput();
  yield jsterm.execute("pprint('hi')");
  yield checkResult("\"  0: \"h\"\n  1: \"i\"\"", "pprint('hi')");

  // check that pprint(function) shows function source, bug 618344
  jsterm.clearOutput();
  yield jsterm.execute("pprint(function() { var someCanaryValue = 42; })");
  yield checkResult(function(node) {
    return node.textContent.indexOf("someCanaryValue") > -1;
  }, "pprint(function) shows source");

  // check that an evaluated null produces "null", bug 650780
  jsterm.clearOutput();
  yield jsterm.execute("null");
  yield checkResult("null", "null is null");

  jsterm.clearOutput();
  yield jsterm.execute("undefined");
  yield checkResult("undefined", "undefined is printed");
}
