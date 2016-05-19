/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the `copy` console helper works as intended.

"use strict";

var gWebConsole, gJSTerm;

var TEXT = "Lorem ipsum dolor sit amet, consectetur adipisicing " +
    "elit, sed do eiusmod tempor incididunt ut labore et dolore magna " +
    "aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco " +
    "laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure " +
    "dolor in reprehenderit in voluptate velit esse cillum dolore eu " +
    "fugiat nulla pariatur. Excepteur sint occaecat cupidatat non " +
    "proident, sunt in culpa qui officia deserunt mollit anim id est laborum." +
    new Date();

var ID = "select-me";

add_task(function* init() {
  yield loadTab("data:text/html;charset=utf-8," +
                "<body>" +
                "  <div>" +
                "    <h1>Testing copy command</h1>" +
                "    <p>This is some example text</p>" +
                "    <p id='select-me'>" + TEXT + "</p>" +
                "  </div>" +
                "  <div><p></p></div>" +
                "</body>");

  gWebConsole = yield openConsole();
  gJSTerm = gWebConsole.jsterm;
});

add_task(function* testCopy() {
  let RANDOM = Math.random();
  let string = "Text: " + RANDOM;
  let obj = {a: 1, b: "foo", c: RANDOM};

  let samples = [
                  [RANDOM, RANDOM],
                  [JSON.stringify(string), string],
                  [obj.toSource(), JSON.stringify(obj, null, "  ")],
    [
      "$('#" + ID + "')",
      content.document.getElementById(ID).outerHTML
    ]
  ];
  for (let [source, reference] of samples) {
    let deferredResult = promise.defer();

    SimpleTest.waitForClipboard(
      "" + reference,
      () => {
        let command = "copy(" + source + ")";
        info("Attempting to copy: " + source);
        info("Executing command: " + command);
        gJSTerm.execute(command, msg => {
          is(msg, undefined, "Command success: " + command);
        });
      },
      deferredResult.resolve,
      deferredResult.reject);

    yield deferredResult.promise;
  }
});

add_task(function* cleanup() {
  gWebConsole = gJSTerm = null;
  gBrowser.removeTab(gBrowser.selectedTab);
  finishTest();
});
