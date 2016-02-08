/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the '%c' modifier works with the console API. See bug 823097.

function test() {
  let hud;

  const TEST_URI = "data:text/html;charset=utf8,<p>test for " +
                   "console.log('%ccustom styles', 'color:red')";

  const checks = [{
    // check the basics work
    style: "color:red;font-size:1.3em",
    props: { color: true, fontSize: true },
    sameStyleExpected: true,
  }, {
    // check that the url() is not allowed
    style: "color:blue;background-image:url('http://example.com/test')",
    props: { color: true, fontSize: false, background: false,
             backgroundImage: false },
    sameStyleExpected: false,
  }, {
    // check that some properties are not allowed
    style: "color:pink;position:absolute;top:10px",
    props: { color: true, position: false, top: false },
    sameStyleExpected: false,
  }];

  Task.spawn(runner).then(finishTest);

  function* runner() {
    const {tab} = yield loadTab(TEST_URI);
    hud = yield openConsole(tab);

    for (let check of checks) {
      yield checkStyle(check);
    }

    yield closeConsole(tab);
  }

  function* checkStyle(check) {
    hud.jsterm.clearOutput();

    info("checkStyle " + check.style);
    hud.jsterm.execute("console.log('%cfoobar', \"" + check.style + "\")");

    let [result] = yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "foobar",
        category: CATEGORY_WEBDEV,
      }],
    });

    let msg = [...result.matched][0];
    ok(msg, "message element");

    let span = msg.querySelector(".message-body span[style]");
    ok(span, "span element");

    info("span textContent is: " + span.textContent);
    isnot(span.textContent.indexOf("foobar"), -1, "span textContent check");

    let outputStyle = span.getAttribute("style").replace(/\s+|;+$/g, "");
    if (check.sameStyleExpected) {
      is(outputStyle, check.style, "span style is correct");
    } else {
      isnot(outputStyle, check.style, "span style is not the same");
    }

    for (let prop of Object.keys(check.props)) {
      is(!!span.style[prop], check.props[prop], "property check for " + prop);
    }
  }
}
